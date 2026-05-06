# Travesty — A Markov Chain Text Generator

A small, dependency-free command-line program that builds a character-level
Markov chain from any input text and uses it to generate new text in a
strikingly similar style.

It is also a deliberate homage to a piece of computing history that, in
retrospect, turns out to be the direct conceptual ancestor of modern large
language models.

```
$ python3 travesty.py -n 6 -c 200 faust.txt
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

## Installation

Requires Python 3.9+. No third-party dependencies.

```bash
git clone https://github.com/<your-user>/travesty.git
cd travesty
python3 travesty.py --help
```

## Usage

```
python3 travesty.py [OPTIONS] FILE [FILE ...]
```

### Options

| Flag | Description | Default |
|------|-------------|---------|
| `FILE` | One or more input files (`.txt` or `.md`). For `.md`, common Markdown formatting markers are stripped on read. | required |
| `-n N`, `--ngram N` | Length of the n-gram context, 1–20. Sweet spot is 4–8. | `5` |
| `-c N`, `--count N` | Number of characters to generate (including the initial seed). | `1000` |
| `-s TEXT`, `--start TEXT` | Initial text the output begins with. Must be at least *n* characters long. | first *n* chars of the input |
| `-o FILE`, `--output FILE` | Write the result to a file instead of stdout. | stdout |
| `--seed N` | Random seed for reproducible output. | system random |

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

Fetch Goethe's *Faust* (German, public domain via Project Gutenberg) and
play with it:

```bash
curl -sL https://www.gutenberg.org/cache/epub/2229/pg2229.txt \
  | sed -n '/^\*\*\* START/,/^\*\*\* END/p' \
  | sed '1d;$d' > faust.txt

# A 1000-character travesty in Goethe's voice
python3 travesty.py -n 6 -c 1000 faust.txt

# Continue a famous opening line
python3 travesty.py -n 7 -c 500 -s "Habe nun, ach! Philosophie, " faust.txt

# Mix multiple sources, write to file, reproducible
python3 travesty.py -n 5 -c 2000 --seed 42 -o out.txt corpus/*.md
```

## Implementations

- **Python** — `travesty.py` (this repository)
- **Rust** — planned
- **Go** — planned

The Rust and Go ports are intended both as performance comparisons and as
an excuse to write the same idea in three quite different languages.

## References

- Borel, É. (1913). *Mécanique Statistique et Irréversibilité.*
- Shannon, C. E. (1948). *A Mathematical Theory of Communication.* Bell
  System Technical Journal.
- Kenner, H. & O'Rourke, J. (1984). *A Travesty Generator for Micros.*
  BYTE Magazine, November 1984.
- Dewdney, A. K. *Computer Recreations* column, *Scientific American* /
  *Spektrum der Wissenschaft* ("Computer-Kurzweil").

## License

MIT — see [LICENSE](LICENSE).
