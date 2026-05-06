#!/usr/bin/env python3
"""Travesty 2 — extended Markov text generator.

Adds three features beyond the basic version:

  --temperature  sharpen (T<1) or smooth (T>1) the follower distribution,
                 mirroring the same idea used by modern LLMs.
  --mode word    build n-grams over words instead of characters,
                 a step closer to how language models actually work.
  --visualize    print a human-readable summary of the n-gram table:
                 the most common prefixes and where they branch.
  --export-table dump the full table as JSON (used by the web demo).
"""

from __future__ import annotations

import argparse
import json
import random
import re
import sys
from collections import Counter, defaultdict
from pathlib import Path
from typing import Iterable


# ----------------------------------------------------------- markdown stripping

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
        raw = p.read_text(encoding="utf-8")
        if p.suffix.lower() == ".md":
            raw = strip_markdown(raw)
        chunks.append(raw)
    return "\n".join(chunks)


# ----------------------------------------------------------------- tokenisation

def tokenise_words(text: str) -> list[str]:
    # Words OR a single non-word, non-whitespace character (punctuation).
    return re.findall(r"\w+|[^\w\s]", text, flags=re.UNICODE)


def join_words(tokens: Iterable[str]) -> str:
    out: list[str] = []
    for i, t in enumerate(tokens):
        if i == 0 or (len(t) == 1 and not t.isalnum() and not t.isspace()):
            out.append(t)
        else:
            out.append(" " + t)
    return "".join(out)


# ------------------------------------------------------------------ table build

def build_table(units: list, n: int) -> dict:
    """Build a {prefix-tuple-or-str: Counter[next]} table.
    Works uniformly for char mode (`units` is a string) and word mode
    (`units` is a list of word tokens)."""
    table: dict = defaultdict(Counter)
    is_string = isinstance(units, str)
    for i in range(len(units) - n):
        prefix = units[i:i + n] if is_string else tuple(units[i:i + n])
        table[prefix][units[i + n]] += 1
    return table


# ---------------------------------------------------------- sampling & generate

def sample_with_temperature(counter: Counter, T: float, rng: random.Random):
    if not counter:
        raise ValueError("empty counter")
    if T <= 0:
        return counter.most_common(1)[0][0]
    items, counts = zip(*counter.items())
    total = sum(counts)
    probs = [c / total for c in counts]
    if T != 1.0:
        scaled = [p ** (1.0 / T) for p in probs]
        s = sum(scaled)
        probs = [x / s for x in scaled]
    return rng.choices(items, weights=probs, k=1)[0]


def slide(prefix, next_unit, mode: str):
    if mode == "char":
        return prefix[1:] + next_unit
    return prefix[1:] + (next_unit,)


def generate(table: dict, units, n: int, count: int,
             start, mode: str, T: float, rng: random.Random):
    if start is None:
        if mode == "char":
            initial = units[:n]
            prefix = initial
            output = [initial]
            produced = len(initial)
        else:
            initial = list(units[:n])
            prefix = tuple(initial)
            output = list(initial)
            produced = len(initial)
    else:
        if mode == "char":
            initial = start
            prefix = start[-n:]
            output = [initial]
            produced = len(initial)
        else:
            start_tokens = tokenise_words(start)
            if len(start_tokens) < n:
                raise ValueError(
                    f"--start has only {len(start_tokens)} tokens, need ≥ n={n}")
            initial = start_tokens
            prefix = tuple(start_tokens[-n:])
            output = list(start_tokens)
            produced = len(start_tokens)

    keys_cache: list | None = None
    while produced < count:
        followers = table.get(prefix)
        if not followers:
            if keys_cache is None:
                keys_cache = list(table.keys())
            prefix = rng.choice(keys_cache)
            continue
        nxt = sample_with_temperature(followers, T, rng)
        if mode == "char":
            output.append(nxt)
            produced += 1
        else:
            output.append(nxt)
            produced += 1
        prefix = slide(prefix, nxt, mode)

    if mode == "char":
        return "".join(output)[:count]
    return join_words(output[:count])


# ------------------------------------------------------------------ visualisation

def visualize(table: dict, mode: str, top_prefixes: int = 12,
              top_followers: int = 6, bar_width: int = 30) -> str:
    lines: list[str] = []
    transitions = sum(sum(c.values()) for c in table.values())
    lines.append(f"Mode:              {mode}")
    lines.append(f"Unique prefixes:   {len(table):>10,}")
    lines.append(f"Total transitions: {transitions:>10,}")
    avg = transitions / len(table) if table else 0
    lines.append(f"Avg followers/prefix: {avg:>7.2f}")
    lines.append("")
    lines.append(f"Top {top_prefixes} prefixes by frequency:")
    lines.append("")
    sorted_prefixes = sorted(
        table.items(),
        key=lambda kv: -sum(kv[1].values()),
    )[:top_prefixes]

    def fmt_unit(u: str) -> str:
        repls = {"\n": "\\n", "\t": "\\t", " ": "·"}
        if isinstance(u, str) and u in repls:
            return repls[u]
        return str(u)

    def fmt_prefix(p) -> str:
        if isinstance(p, str):
            return "".join(fmt_unit(c) for c in p)
        return " ".join(fmt_unit(t) for t in p)

    for prefix, followers in sorted_prefixes:
        total = sum(followers.values())
        lines.append(f'  "{fmt_prefix(prefix)}"   ({total:,})')
        for follower, cnt in followers.most_common(top_followers):
            pct = 100.0 * cnt / total
            bar = "█" * int(round(pct / 100 * bar_width))
            lines.append(f"      → {fmt_unit(follower):<8} {pct:5.1f}%  {bar}")
        if len(followers) > top_followers:
            lines.append(f"      → … ({len(followers) - top_followers} more)")
        lines.append("")
    return "\n".join(lines)


# --------------------------------------------------------------- JSON export

def export_table_json(table: dict, mode: str, n: int) -> str:
    serial: dict = {
        "mode": mode,
        "n": n,
        "table": {},
    }
    for prefix, followers in table.items():
        key = prefix if isinstance(prefix, str) else " ".join(prefix)
        serial["table"][key] = dict(followers)
    return json.dumps(serial, ensure_ascii=False)


# ---------------------------------------------------------------- argparse

def parse_args(argv: list[str] | None = None) -> argparse.Namespace:
    p = argparse.ArgumentParser(
        description=("Extended Markov text generator with temperature, word "
                     "n-grams, and table visualisation."),
    )
    p.add_argument("files", nargs="+", type=Path,
                   help="One or more input files (.txt or .md).")
    p.add_argument("-n", "--ngram", type=int, default=5, metavar="N",
                   help="Length of the n-gram context (default 5).")
    p.add_argument("-c", "--count", type=int, default=1000, metavar="N",
                   help="Number of units (chars or words) to generate (default 1000).")
    p.add_argument("-s", "--start", type=str, metavar="TEXT",
                   help="Initial text the output begins with.")
    p.add_argument("-o", "--output", type=Path, metavar="DATEI",
                   help="Output file (default stdout).")
    p.add_argument("-t", "--temperature", type=float, default=1.0, metavar="T",
                   help=("Sampling temperature. 1.0 = unmodified frequencies, "
                         "<1 sharper (more deterministic), >1 smoother "
                         "(more random), 0 = always pick the most frequent "
                         "follower (default 1.0)."))
    p.add_argument("-m", "--mode", choices=("char", "word"), default="char",
                   help="N-gram mode: per character (default) or per word.")
    p.add_argument("-v", "--visualize", action="store_true",
                   help="Print a summary of the n-gram table instead of "
                        "generating text.")
    p.add_argument("--export-table", type=Path, metavar="FILE",
                   help="Write the n-gram table as JSON to FILE and exit.")
    p.add_argument("--seed", type=int,
                   help="Random seed for reproducible output.")
    return p.parse_args(argv)


# ---------------------------------------------------------------------- main

def main(argv: list[str] | None = None) -> int:
    args = parse_args(argv)

    if not 1 <= args.ngram <= 50:
        print("Error: --ngram must be between 1 and 50.", file=sys.stderr)
        return 2
    if args.count < 1:
        print("Error: --count must be > 0.", file=sys.stderr)
        return 2
    if args.temperature < 0:
        print("Error: --temperature must be >= 0.", file=sys.stderr)
        return 2

    text = load_texts(args.files)
    if len(text) == 0:
        print("Error: empty input.", file=sys.stderr)
        return 1

    if args.mode == "char":
        units = text
    else:
        units = tokenise_words(text)

    if len(units) <= args.ngram:
        print(f"Error: input too short ({len(units)} units) for n={args.ngram}.",
              file=sys.stderr)
        return 1

    rng = random.Random(args.seed) if args.seed is not None else random.Random()

    table = build_table(units, args.ngram)

    if args.export_table:
        args.export_table.write_text(
            export_table_json(table, args.mode, args.ngram),
            encoding="utf-8",
        )
        return 0

    if args.visualize:
        result = visualize(table, args.mode)
        if args.output:
            args.output.write_text(result + "\n", encoding="utf-8")
        else:
            print(result)
        return 0

    try:
        result = generate(
            table, units, args.ngram, args.count,
            args.start, args.mode, args.temperature, rng,
        )
    except ValueError as e:
        print(f"Error: {e}", file=sys.stderr)
        return 2

    if args.output:
        args.output.write_text(result, encoding="utf-8")
    else:
        sys.stdout.write(result)
        if not result.endswith("\n"):
            sys.stdout.write("\n")
    return 0


if __name__ == "__main__":
    sys.exit(main())
