// Travesty — character-level Markov chain text generator.
package main

import (
	"flag"
	"fmt"
	"math/rand"
	"os"
	"path/filepath"
	"regexp"
	"strings"
	"time"
)

func main() {
	os.Exit(run())
}

func run() int {
	var (
		ngram  int
		count  int
		start  string
		output string
		seed   int64
	)
	flag.IntVar(&ngram, "n", 5, "n-gram context length (1-20)")
	flag.IntVar(&ngram, "ngram", 5, "n-gram context length (1-20)")
	flag.IntVar(&count, "c", 1000, "number of characters to generate")
	flag.IntVar(&count, "count", 1000, "number of characters to generate")
	flag.StringVar(&start, "s", "", "initial text the output begins with (>= n chars)")
	flag.StringVar(&start, "start", "", "initial text the output begins with (>= n chars)")
	flag.StringVar(&output, "o", "", "output file (default stdout)")
	flag.StringVar(&output, "output", "", "output file (default stdout)")
	flag.Int64Var(&seed, "seed", 0, "random seed (0 = system random)")

	flag.Usage = func() {
		fmt.Fprintf(os.Stderr,
			"Usage: %s [OPTIONS] FILE [FILE ...]\n\n"+
				"Character-level Markov chain text generator (n-gram travesty).\n\n"+
				"Options:\n", os.Args[0])
		flag.PrintDefaults()
	}
	flag.Parse()

	files := flag.Args()
	if len(files) == 0 {
		flag.Usage()
		return 2
	}
	if ngram < 1 || ngram > 20 {
		fmt.Fprintln(os.Stderr, "Error: -ngram must be between 1 and 20.")
		return 2
	}
	if count < 1 {
		fmt.Fprintln(os.Stderr, "Error: -count must be > 0.")
		return 2
	}
	if start != "" && len([]rune(start)) < ngram {
		fmt.Fprintf(os.Stderr,
			"Error: -start is shorter than n=%d (%d chars).\n",
			ngram, len([]rune(start)))
		return 2
	}

	text, err := loadTexts(files)
	if err != nil {
		fmt.Fprintln(os.Stderr, "Read error:", err)
		return 1
	}

	runes := []rune(text)
	if len(runes) <= ngram {
		fmt.Fprintf(os.Stderr,
			"Error: input too short (%d chars) for n=%d.\n",
			len(runes), ngram)
		return 1
	}

	var rng *rand.Rand
	if seed != 0 {
		rng = rand.New(rand.NewSource(seed))
	} else {
		rng = rand.New(rand.NewSource(time.Now().UnixNano()))
	}

	table := buildTable(runes, ngram)

	if start != "" {
		startRunes := []rune(start)
		lastN := string(startRunes[len(startRunes)-ngram:])
		if _, ok := table[lastN]; !ok {
			fmt.Fprintln(os.Stderr,
				"Warning: trailing context of -start not found in corpus; "+
					"continuing with random context after the initial text.")
		}
	}

	result := generate(table, runes, ngram, count, start, rng)

	if output != "" {
		if err := os.WriteFile(output, []byte(result), 0644); err != nil {
			fmt.Fprintln(os.Stderr, "Write error:", err)
			return 1
		}
	} else {
		fmt.Print(result)
		if !strings.HasSuffix(result, "\n") {
			fmt.Println()
		}
	}
	return 0
}

func stripMarkdown(text string) string {
	patterns := [][2]string{
		{"(?s)```.*?```", ""},
		{"`[^`\n]*`", ""},
		{`!\[[^\]]*\]\([^)]*\)`, ""},
		{`\[([^\]]*)\]\([^)]*\)`, "$1"},
		{`<[^>]+>`, ""},
		{`(?m)^[ \t]*(#{1,6}|>|[-*+]|\d+\.)\s+`, ""},
		{`(?m)^[-*_]{3,}\s*$`, ""},
		{`\*{1,3}([^*\n]+)\*{1,3}`, "$1"},
		{`_{1,3}([^_\n]+)_{1,3}`, "$1"},
	}
	for _, p := range patterns {
		re := regexp.MustCompile(p[0])
		text = re.ReplaceAllString(text, p[1])
	}
	return text
}

func loadTexts(paths []string) (string, error) {
	chunks := make([]string, 0, len(paths))
	for _, p := range paths {
		raw, err := os.ReadFile(p)
		if err != nil {
			return "", err
		}
		s := string(raw)
		if strings.ToLower(filepath.Ext(p)) == ".md" {
			s = stripMarkdown(s)
		}
		chunks = append(chunks, s)
	}
	return strings.Join(chunks, "\n"), nil
}

func buildTable(runes []rune, n int) map[string][]rune {
	table := make(map[string][]rune)
	if len(runes) <= n {
		return table
	}
	for i := 0; i <= len(runes)-n-1; i++ {
		prefix := string(runes[i : i+n])
		table[prefix] = append(table[prefix], runes[i+n])
	}
	return table
}

func generate(
	table map[string][]rune,
	seedRunes []rune,
	n, count int,
	start string,
	rng *rand.Rand,
) string {
	var initial []rune
	if start != "" {
		initial = []rune(start)
	} else {
		initial = append([]rune(nil), seedRunes[:n]...)
	}

	out := make([]rune, len(initial), count+n)
	copy(out, initial)

	var window []rune
	if len(initial) >= n {
		window = append([]rune(nil), initial[len(initial)-n:]...)
	} else {
		window = append([]rune(nil), initial...)
	}

	var keys []string

	for len(out) < count {
		followers, ok := table[string(window)]
		if !ok || len(followers) == 0 {
			if keys == nil {
				keys = make([]string, 0, len(table))
				for k := range table {
					keys = append(keys, k)
				}
			}
			if len(keys) == 0 {
				break
			}
			window = []rune(keys[rng.Intn(len(keys))])
			continue
		}
		next := followers[rng.Intn(len(followers))]
		out = append(out, next)
		if len(window) >= n {
			window = append(window[1:], next)
		} else {
			window = append(window, next)
		}
	}

	if len(out) > count {
		out = out[:count]
	}
	return string(out)
}
