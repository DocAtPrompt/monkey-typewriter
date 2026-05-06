#!/usr/bin/env bash
# Download public-domain texts from Project Gutenberg for travesty experiments.
# Add your own by calling fetch_pg with the book's PG ID and a target filename.

set -euo pipefail
cd "$(dirname "$0")"

fetch_pg() {
    local id="$1"
    local out="$2"
    local url="https://www.gutenberg.org/cache/epub/${id}/pg${id}.txt"
    echo "→ ${out}  (${url})"
    curl -fsSL "$url" \
        | sed -n '/^\*\*\* START OF/,/^\*\*\* END OF/p' \
        | sed '1d;$d' \
        > "$out"
}

# Goethe — Faust I (German)
fetch_pg 2229 faust.txt

# Uncomment to add more (verify the PG ID first at gutenberg.org):
# fetch_pg 100   shakespeare.txt    # Shakespeare, complete works (English)
# fetch_pg 2000  don_quixote.txt    # Cervantes, Don Quixote (Spanish)
# fetch_pg 1342  pride_prejudice.txt # Austen, Pride and Prejudice (English)

echo
echo "Done. Try one of:"
echo "  python3 ../python/travesty.py -n 6 -c 800 faust.txt"
echo "  ../rust/target/release/travesty -n 6 -c 800 faust.txt"
echo "  ../go/travesty -n 6 -c 800 faust.txt"
