#!/usr/bin/env python3
"""Markov-/Travesty-Textgenerator auf Zeichen-n-Gramm-Basis.

Inspiriert vom 'Travesty Generator' (Kenner & O'Rourke, BYTE 1984), der
in den 80ern auch in der Spektrum-Kolumne 'Computer-Kurzweil' erschien.
"""

from __future__ import annotations

import argparse
import random
import re
import sys
from collections import defaultdict
from pathlib import Path


def strip_markdown(text: str) -> str:
    text = re.sub(r"```.*?```", "", text, flags=re.DOTALL)
    text = re.sub(r"`[^`\n]*`", "", text)
    text = re.sub(r"!\[[^\]]*\]\([^)]*\)", "", text)
    text = re.sub(r"\[([^\]]*)\]\([^)]*\)", r"\1", text)
    text = re.sub(r"<[^>]+>", "", text)
    text = re.sub(r"^[ \t]*(#{1,6}|>|[-*+]|\d+\.)\s+", "", text, flags=re.MULTILINE)
    text = re.sub(r"^[-*_]{3,}\s*$", "", text, flags=re.MULTILINE)
    text = re.sub(r"\*{1,3}([^*\n]+)\*{1,3}", r"\1", text)
    text = re.sub(r"_{1,3}([^_\n]+)_{1,3}", r"\1", text)
    return text


def load_texts(paths: list[Path]) -> str:
    chunks: list[str] = []
    for p in paths:
        if not p.is_file():
            raise FileNotFoundError(f"Datei nicht gefunden: {p}")
        raw = p.read_text(encoding="utf-8")
        if p.suffix.lower() == ".md":
            raw = strip_markdown(raw)
        chunks.append(raw)
    return "\n".join(chunks)


def build_ngram_table(text: str, n: int) -> dict[str, list[str]]:
    # Liste mit Mehrfachzählung statt gewichtetem Dict: random.choice trifft
    # damit die Häufigkeitsverteilung implizit und ist deutlich schneller
    # als choices() mit weights bei jeder Abfrage.
    table: dict[str, list[str]] = defaultdict(list)
    for i in range(len(text) - n):
        table[text[i:i + n]].append(text[i + n])
    return table


def generate(
    table: dict[str, list[str]],
    seed_text: str,
    n: int,
    count: int,
    start: str | None = None,
) -> str:
    if start is not None:
        initial = start
        prefix = start[-n:]
    else:
        initial = seed_text[:n]
        prefix = initial
    out: list[str] = [initial]
    produced = len(initial)
    keys: list[str] | None = None
    while produced < count:
        followers = table.get(prefix)
        if not followers:
            if keys is None:
                keys = list(table.keys())
            prefix = random.choice(keys)
            continue
        nxt = random.choice(followers)
        out.append(nxt)
        produced += 1
        prefix = prefix[1:] + nxt
    return "".join(out)[:count]


def parse_args(argv: list[str] | None = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description=(
            "Markov-Textgenerator: erzeugt aus einem Korpus per Zeichen-"
            "n-Gramm-Statistik neuen, stilistisch ähnlichen Text."
        ),
    )
    parser.add_argument(
        "files",
        nargs="+",
        type=Path,
        help="Eine oder mehrere Eingabedateien (.txt oder .md). Bei .md "
             "werden gängige Markdown-Formatzeichen entfernt.",
    )
    parser.add_argument(
        "-n", "--ngram",
        type=int,
        default=5,
        metavar="N",
        help="Länge des Kontexts in Zeichen (1-20). Klein = chaotisch, "
             "groß = nah am Original. Default: 5.",
    )
    parser.add_argument(
        "-c", "--count",
        type=int,
        default=1000,
        metavar="N",
        help="Anzahl der zu erzeugenden Zeichen. Default: 1000.",
    )
    parser.add_argument(
        "-o", "--output",
        type=Path,
        metavar="DATEI",
        help="Ausgabedatei. Ohne Angabe wird auf stdout geschrieben.",
    )
    parser.add_argument(
        "-s", "--start",
        type=str,
        metavar="TEXT",
        help="Initialtext, mit dem die Ausgabe beginnt (mindestens n Zeichen "
             "lang). Ohne Angabe werden die ersten n Zeichen des Korpus "
             "als Anfang verwendet.",
    )
    parser.add_argument(
        "--seed",
        type=int,
        help="Optionaler Zufalls-Seed für reproduzierbare Ergebnisse.",
    )
    return parser.parse_args(argv)


def main(argv: list[str] | None = None) -> int:
    args = parse_args(argv)

    if not 1 <= args.ngram <= 20:
        print("Fehler: --ngram muss zwischen 1 und 20 liegen.", file=sys.stderr)
        return 2
    if args.count < 1:
        print("Fehler: --count muss > 0 sein.", file=sys.stderr)
        return 2
    if args.start is not None and len(args.start) < args.ngram:
        print(
            f"Fehler: --start ist kürzer als n={args.ngram} "
            f"({len(args.start)} Zeichen).",
            file=sys.stderr,
        )
        return 2
    if args.seed is not None:
        random.seed(args.seed)

    try:
        text = load_texts(args.files)
    except (OSError, UnicodeDecodeError) as e:
        print(f"Fehler beim Lesen: {e}", file=sys.stderr)
        return 1

    if len(text) <= args.ngram:
        print(
            f"Fehler: Eingabetext zu kurz ({len(text)} Zeichen) "
            f"für n={args.ngram}.",
            file=sys.stderr,
        )
        return 1

    table = build_ngram_table(text, args.ngram)

    if args.start is not None and args.start[-args.ngram:] not in table:
        print(
            f"Warnung: Kontext '{args.start[-args.ngram:]}' kommt im "
            f"Korpus nicht vor; nach dem Initialtext wird mit zufälligem "
            f"Kontext fortgesetzt.",
            file=sys.stderr,
        )

    result = generate(table, text, args.ngram, args.count, args.start)

    if args.output:
        args.output.write_text(result, encoding="utf-8")
    else:
        sys.stdout.write(result)
        if not result.endswith("\n"):
            sys.stdout.write("\n")
    return 0


if __name__ == "__main__":
    sys.exit(main())
