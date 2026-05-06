# Travesty — A Markov Chain Text Generator

A small, dependency-light command-line program that builds a character-level
Markov chain from any input text and uses it to generate new text in a
strikingly similar style. Available in three implementations: **Python**,
**Rust**, and **Go**.

It is also a deliberate homage to a piece of computing history that, in
retrospect, turns out to be the direct conceptual ancestor of modern large
language models.

```
$ travesty -n 6 -c 200 corpus/faust.txt
Faust und fernen darauf mich von deinem lange hab ich jetzo bitt ich Eurer
Lüsternheit, Und für dich in der Natur. MEPHISTOPHELES (allein. DIE TIERE.
So hört doch zuletzt bringt Gefahr, ich lieber. MEPHISTOPHELES. Still, er
wie er sie aller Hörer zwingt
```

## A short biography of this program

### 1913 — The infinite monkey

The story begins with a thought experiment popularized by the French
mathematician **Émile Borel**: a monkey hitting random keys on a typewriter
for an infinite amount of time will, with probability 1, eventually type any
given text — say, the complete works of Shakespeare. Borel's point was *not*
that this is feasible but to illustrate just how vanishingly small "almost
certain" can be in practice.

### 1948 — Claude Shannon refines the monkey

In his foundational paper *A Mathematical Theory of Communication*,
**Claude Shannon** asked a sharper question: what if the monkey doesn't type
uniformly at random, but samples letters according to their statistical
frequency in real English? He went further still: weight pairs of letters
(bigrams), then triples (trigrams), then whole words. The output
progressively starts to *look* like English — even though no rule of grammar
or meaning is involved. This was the birth of statistical language modeling.

### 1984 — The Travesty Generator

In the November 1984 issue of *BYTE Magazine*, **Hugh Kenner and Joseph
O'Rourke** published "A Travesty Generator for Micros" — a small program
that did exactly what Shannon described, but on an 8-bit home computer.
Feed it Hemingway and out came pseudo-Hemingway; feed it Kant and out came
pseudo-Kant.

The article was widely picked up. **A. K. Dewdney** covered it in
*Scientific American*'s "Computer Recreations" column, and the German
translation appeared in *Spektrum der Wissenschaft* under the title
**"Computer-Kurzweil"**. A whole generation of hobbyists typed BASIC
listings of this algorithm into their machines and discovered, to their
astonishment, that pure character statistics carry a surprising amount of
an author's voice.

This program is a direct descendant of those listings — same algorithm,
modern syntax.

### 2017+ — Modern Large Language Models

The line from Shannon's hand-tabulated bigrams through the BASIC monkey of
*Computer-Kurzweil* to today's LLMs is direct. GPT, Claude, Llama and the
rest are doing the same thing the Travesty generator does — predicting the
next token from the preceding context — only with two changes:

1. The "context" is no longer 5–8 characters but thousands of tokens.
2. The conditional probabilities are no longer counted from a corpus but
   *learned* by a neural network with billions of parameters.

But conceptually, an LLM is the great-great-grandchild of the BASIC monkey.
This program is a tribute to that ancestor — and a small, self-contained
way to feel where the magic starts and where it stops.

## Demo

The same Faust corpus, the same seed, four different *n* values:

### n = 2 (letter pairs only)
```
Diel sich reintwas Glauteiselt! Gottescht Perscht es undiels Fesehlum
ihmeineittim Ster Brugib ich inen frei, Phat Sin ist ver,
SIEBELES.
```
Texture is right, hardly any real words.

### n = 4
```
Marthe!
Juchhe!
Juchherrlichen vorm rogatur.
Da saßen stillet Bach rechenkt, ausgespitzes, frisch gebracht!
```
Real words appear, character names emerge, no grammar yet.

### n = 6
```
Faust und fernen darauf mich von deinem lange hab ich jetzo bitt ich
Eurer Lüsternheit, Und für dich in der Natur.
MEPHISTOPHELES (allein.
```
Sentences nearly grammatical, stage directions in plausible places,
Goethe's voice clearly recognisable.

### n = 8
```
Faust:
Der Tragödie ersten Ostertage,
Erlaubt Ihr mir vorbei und schäumt das Meer sich mit behende,
Führt mir nach des Lebens Bächen,
Ach! nach dem Tische wendend.)
```
Almost lyrical, on the edge of meaningful — yet still pure statistics.

## Repository layout

```
.
├── python/      Python 3 reference implementation (no dependencies)
├── rust/        Rust port (clap + rand + regex)
├── go/          Go port (standard library only)
└── corpus/      Helper script for fetching public-domain sample texts
```

## Quick start

```bash
# 1. Clone
git clone https://github.com/DocAtPrompt/monkey-typewriter.git
cd monkey-typewriter

# 2. Fetch a sample corpus (Goethe's Faust)
bash corpus/fetch.sh

# 3. Run any of the three implementations
python3 python/travesty.py -n 6 -c 800 corpus/faust.txt

(cd rust && cargo build --release)
./rust/target/release/travesty -n 6 -c 800 corpus/faust.txt

(cd go && go build -o travesty .)
./go/travesty -n 6 -c 800 corpus/faust.txt
```

### Build details

| Implementation | Requirements | Build |
|----------------|-------------|-------|
| Python  | Python 3.9+, no third-party packages | none — run the script |
| Rust    | Rust 1.70+ (`cargo`)                  | `cd rust && cargo build --release` |
| Go      | Go 1.21+                              | `cd go && go build -o travesty .` |

## Usage

All three implementations share the same options.

### Options

| Flag | Description | Default |
|------|-------------|---------|
| `FILE` | One or more input files (`.txt` or `.md`). For `.md`, common Markdown formatting markers are stripped on read. | required |
| `-n N`, `--ngram N` *(`-ngram` in Go)* | Length of the n-gram context, 1–20. | `5` |
| `-c N`, `--count N` *(`-count` in Go)* | Number of characters to generate (including the initial seed). | `1000` |
| `-s TEXT`, `--start TEXT` *(`-start` in Go)* | Initial text the output begins with. Must be at least *n* characters long. | first *n* chars of input |
| `-o FILE`, `--output FILE` *(`-output` in Go)* | Write output to a file instead of stdout. | stdout |
| `--seed N` *(`-seed` in Go)* | Random seed for reproducible output. | system random |

### Choosing *n*

The order *n* of the Markov chain controls how much "memory" the generator
has. Roughly:

| n | What you get |
|---|--------------|
| 1 | Letter-frequency noise. Right *texture* of the language, no real words. |
| 2 | Pronounceable nonsense, occasional real syllables. |
| 3 | Many real words, no grammar. |
| 4–5 | Real words, plausible word boundaries, fragments of sense. |
| 6–7 | Grammatical fragments, the author's voice clearly recognisable. |
| 8–10 | Nearly grammatical sentences; output begins to overlap heavily with the input. |
| 11+ | Mostly verbatim quotation of the source. |

The interesting zone — where the generator sounds *like* the author without
simply quoting them — is typically between 5 and 8.

## Examples

```bash
# A 1000-character travesty in Goethe's voice
python3 python/travesty.py -n 6 -c 1000 corpus/faust.txt

# Continue a famous opening line
python3 python/travesty.py -n 7 -c 500 -s "Habe nun, ach! Philosophie, " corpus/faust.txt

# Mix multiple sources, write to file, reproducible
python3 python/travesty.py -n 5 -c 2000 --seed 42 -o out.txt corpus/*.txt
```

## References

- Borel, É. (1913). *Mécanique Statistique et Irréversibilité.*
- Shannon, C. E. (1948). *A Mathematical Theory of Communication.* Bell
  System Technical Journal, 27(3), 379–423.
- Kenner, H. & O'Rourke, J. (1984). *A Travesty Generator for Micros.*
  BYTE Magazine, November 1984.
- Dewdney, A. K. *Computer Recreations* column, *Scientific American* /
  *Spektrum der Wissenschaft* ("Computer-Kurzweil").

## License

MIT — see [LICENSE](LICENSE).
