use clap::Parser;
use rand::rngs::StdRng;
use rand::seq::SliceRandom;
use rand::SeedableRng;
use regex::Regex;
use std::collections::HashMap;
use std::fs;
use std::path::PathBuf;
use std::process::ExitCode;

#[derive(Parser, Debug)]
#[command(
    name = "travesty",
    about = "Character-level Markov chain text generator (n-gram travesty).",
    long_about = None,
)]
struct Args {
    /// Input files (.txt or .md). For .md, common Markdown markers are stripped.
    #[arg(required = true)]
    files: Vec<PathBuf>,

    /// Length of the n-gram context, 1-20 (sweet spot 4-8).
    #[arg(short = 'n', long = "ngram", default_value_t = 5)]
    ngram: usize,

    /// Number of characters to generate (including the initial seed).
    #[arg(short = 'c', long = "count", default_value_t = 1000)]
    count: usize,

    /// Initial text the output begins with (must be at least n chars).
    #[arg(short = 's', long = "start")]
    start: Option<String>,

    /// Write output to file instead of stdout.
    #[arg(short = 'o', long = "output")]
    output: Option<PathBuf>,

    /// Random seed for reproducible output.
    #[arg(long)]
    seed: Option<u64>,
}

fn strip_markdown(text: &str) -> String {
    let patterns: &[(&str, &str)] = &[
        (r"(?s)```.*?```", ""),
        (r"`[^`\n]*`", ""),
        (r"!\[[^\]]*\]\([^)]*\)", ""),
        (r"\[([^\]]*)\]\([^)]*\)", "$1"),
        (r"<[^>]+>", ""),
        (r"(?m)^[ \t]*(#{1,6}|>|[-*+]|\d+\.)\s+", ""),
        (r"(?m)^[-*_]{3,}\s*$", ""),
        (r"\*{1,3}([^*\n]+)\*{1,3}", "$1"),
        (r"_{1,3}([^_\n]+)_{1,3}", "$1"),
    ];
    let mut s = text.to_string();
    for (pat, rep) in patterns {
        let re = Regex::new(pat).expect("invalid regex pattern");
        s = re.replace_all(&s, *rep).into_owned();
    }
    s
}

fn load_texts(paths: &[PathBuf]) -> std::io::Result<String> {
    let mut chunks = Vec::with_capacity(paths.len());
    for p in paths {
        let raw = fs::read_to_string(p)?;
        let processed = if p.extension().and_then(|s| s.to_str()) == Some("md") {
            strip_markdown(&raw)
        } else {
            raw
        };
        chunks.push(processed);
    }
    Ok(chunks.join("\n"))
}

fn build_table(chars: &[char], n: usize) -> HashMap<Vec<char>, Vec<char>> {
    let mut table: HashMap<Vec<char>, Vec<char>> = HashMap::new();
    if chars.len() <= n {
        return table;
    }
    for i in 0..chars.len() - n {
        let prefix = chars[i..i + n].to_vec();
        table.entry(prefix).or_default().push(chars[i + n]);
    }
    table
}

fn generate(
    table: &HashMap<Vec<char>, Vec<char>>,
    seed_chars: &[char],
    n: usize,
    count: usize,
    start: Option<&str>,
    rng: &mut StdRng,
) -> String {
    let initial: Vec<char> = match start {
        Some(s) => s.chars().collect(),
        None => seed_chars[..n].to_vec(),
    };
    let mut prefix: Vec<char> = if initial.len() >= n {
        initial[initial.len() - n..].to_vec()
    } else {
        initial.clone()
    };
    let mut output: Vec<char> = initial;
    let mut keys_cache: Option<Vec<&Vec<char>>> = None;

    while output.len() < count {
        let next = match table.get(&prefix) {
            Some(followers) if !followers.is_empty() => *followers.choose(rng).unwrap(),
            _ => {
                let keys =
                    keys_cache.get_or_insert_with(|| table.keys().collect::<Vec<_>>());
                if keys.is_empty() {
                    break;
                }
                prefix = (*keys.choose(rng).unwrap()).clone();
                continue;
            }
        };
        output.push(next);
        if prefix.len() >= n {
            prefix.remove(0);
        }
        prefix.push(next);
    }

    output.truncate(count);
    output.into_iter().collect()
}

fn main() -> ExitCode {
    let args = Args::parse();

    if !(1..=20).contains(&args.ngram) {
        eprintln!("Error: --ngram must be between 1 and 20.");
        return ExitCode::from(2);
    }
    if args.count < 1 {
        eprintln!("Error: --count must be > 0.");
        return ExitCode::from(2);
    }
    if let Some(ref s) = args.start {
        if s.chars().count() < args.ngram {
            eprintln!(
                "Error: --start is shorter than n={} ({} chars).",
                args.ngram,
                s.chars().count()
            );
            return ExitCode::from(2);
        }
    }

    let text = match load_texts(&args.files) {
        Ok(t) => t,
        Err(e) => {
            eprintln!("Read error: {}", e);
            return ExitCode::from(1);
        }
    };

    let chars: Vec<char> = text.chars().collect();
    if chars.len() <= args.ngram {
        eprintln!(
            "Error: input too short ({} chars) for n={}.",
            chars.len(),
            args.ngram
        );
        return ExitCode::from(1);
    }

    let mut rng = match args.seed {
        Some(s) => StdRng::seed_from_u64(s),
        None => StdRng::from_entropy(),
    };

    let table = build_table(&chars, args.ngram);

    if let Some(ref s) = args.start {
        let last_n: Vec<char> = s.chars().skip(s.chars().count() - args.ngram).collect();
        if !table.contains_key(&last_n) {
            eprintln!(
                "Warning: trailing context of --start not found in corpus; \
                 continuing with random context after the initial text."
            );
        }
    }

    let result = generate(
        &table,
        &chars,
        args.ngram,
        args.count,
        args.start.as_deref(),
        &mut rng,
    );

    match args.output {
        Some(path) => {
            if let Err(e) = fs::write(&path, &result) {
                eprintln!("Write error: {}", e);
                return ExitCode::from(1);
            }
        }
        None => {
            print!("{}", result);
            if !result.ends_with('\n') {
                println!();
            }
        }
    }
    ExitCode::SUCCESS
}
