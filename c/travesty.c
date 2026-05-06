/*
 * travesty.c - Character-level Markov chain text generator (C11).
 *
 * Builds an n-gram table from one or more input files and generates new
 * text in the style of the input. UTF-8 aware. No third-party deps.
 *
 * The n-gram table is kept as a sorted array of (prefix, follower) pairs.
 * Lookup is binary search; sampling picks a random entry in the matching
 * range. This is simpler than a hash table and plenty fast for the texts
 * one would realistically feed into a travesty generator.
 */

#define _POSIX_C_SOURCE 200809L

#include <ctype.h>
#include <errno.h>
#include <getopt.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

typedef uint32_t cp_t;

typedef struct {
    const cp_t *prefix;
    cp_t        next;
} entry_t;

static int g_n = 5;

/* ------------------------------------------------------------------ UTF-8 */

static int utf8_decode_one(const unsigned char *p, size_t avail, cp_t *out) {
    if (avail == 0) return 0;
    unsigned b0 = p[0];
    if (b0 < 0x80) { *out = b0; return 1; }
    if ((b0 & 0xE0) == 0xC0 && avail >= 2) {
        *out = ((b0 & 0x1F) << 6) | (p[1] & 0x3F);
        return 2;
    }
    if ((b0 & 0xF0) == 0xE0 && avail >= 3) {
        *out = ((b0 & 0x0F) << 12) | ((p[1] & 0x3F) << 6) | (p[2] & 0x3F);
        return 3;
    }
    if ((b0 & 0xF8) == 0xF0 && avail >= 4) {
        *out = ((b0 & 0x07) << 18) | ((p[1] & 0x3F) << 12)
             | ((p[2] & 0x3F) << 6) | (p[3] & 0x3F);
        return 4;
    }
    *out = 0xFFFD;
    return 1;
}

static int utf8_encode_one(cp_t cp, char *out) {
    if (cp < 0x80) { out[0] = (char)cp; return 1; }
    if (cp < 0x800) {
        out[0] = (char)(0xC0 | (cp >> 6));
        out[1] = (char)(0x80 | (cp & 0x3F));
        return 2;
    }
    if (cp < 0x10000) {
        out[0] = (char)(0xE0 | (cp >> 12));
        out[1] = (char)(0x80 | ((cp >> 6) & 0x3F));
        out[2] = (char)(0x80 | (cp & 0x3F));
        return 3;
    }
    out[0] = (char)(0xF0 | (cp >> 18));
    out[1] = (char)(0x80 | ((cp >> 12) & 0x3F));
    out[2] = (char)(0x80 | ((cp >> 6) & 0x3F));
    out[3] = (char)(0x80 | (cp & 0x3F));
    return 4;
}

static cp_t *utf8_decode_all(const char *bytes, size_t blen, size_t *out_count) {
    cp_t *cps = malloc(blen * sizeof(cp_t));
    if (!cps) return NULL;
    size_t i = 0, n = 0;
    while (i < blen) {
        cp_t cp;
        int adv = utf8_decode_one((const unsigned char *)bytes + i, blen - i, &cp);
        if (adv <= 0) break;
        cps[n++] = cp;
        i += (size_t)adv;
    }
    *out_count = n;
    return cps;
}

/* -------------------------------------------------------------- file I/O */

static char *read_file(const char *path, size_t *out_len) {
    FILE *fp = fopen(path, "rb");
    if (!fp) return NULL;
    if (fseek(fp, 0, SEEK_END) != 0) { fclose(fp); return NULL; }
    long sz = ftell(fp);
    if (sz < 0) { fclose(fp); return NULL; }
    rewind(fp);
    char *buf = malloc((size_t)sz + 1);
    if (!buf) { fclose(fp); return NULL; }
    size_t r = fread(buf, 1, (size_t)sz, fp);
    fclose(fp);
    buf[r] = '\0';
    *out_len = r;
    return buf;
}

/* -------------------------------------------------------- Markdown strip */

/* Hand-rolled byte-level stripper. Handles the common cases: code blocks
 * (``` … ```), inline code, headers, list/blockquote markers at line start,
 * bold/italic markers, links, images, and HTML tags. Operates on UTF-8
 * bytes — the markers themselves are all ASCII so this is safe. */
static char *strip_markdown(const char *in, size_t len, size_t *out_len) {
    char *out = malloc(len + 1);
    if (!out) return NULL;
    size_t oi = 0, i = 0;
    int at_line_start = 1;

    while (i < len) {
        /* Code fence: ``` ... ``` (multiline) */
        if (i + 2 < len && in[i] == '`' && in[i+1] == '`' && in[i+2] == '`') {
            i += 3;
            while (i + 2 < len && !(in[i] == '`' && in[i+1] == '`' && in[i+2] == '`'))
                i++;
            if (i + 2 < len) i += 3;
            at_line_start = 0;
            continue;
        }
        /* Inline code: `...` (no newline) */
        if (in[i] == '`') {
            i++;
            while (i < len && in[i] != '`' && in[i] != '\n') i++;
            if (i < len && in[i] == '`') i++;
            at_line_start = 0;
            continue;
        }
        /* Line-leading markers: skip whitespace + header/list/blockquote/ordered-list. */
        if (at_line_start) {
            size_t j = i;
            while (j < len && (in[j] == ' ' || in[j] == '\t')) j++;
            if (j < len && in[j] == '#') {
                while (j < len && in[j] == '#') j++;
                while (j < len && in[j] == ' ') j++;
                i = j; at_line_start = 0; continue;
            }
            if (j < len && in[j] == '>') {
                j++;
                while (j < len && in[j] == ' ') j++;
                i = j; at_line_start = 0; continue;
            }
            if (j + 1 < len && (in[j] == '-' || in[j] == '*' || in[j] == '+')
                && in[j+1] == ' ') {
                i = j + 2; at_line_start = 0; continue;
            }
            if (j < len && in[j] >= '0' && in[j] <= '9') {
                size_t k = j;
                while (k < len && in[k] >= '0' && in[k] <= '9') k++;
                if (k + 1 < len && in[k] == '.' && in[k+1] == ' ') {
                    i = k + 2; at_line_start = 0; continue;
                }
            }
        }
        /* Image: ![alt](url) - removed entirely */
        if (in[i] == '!' && i + 1 < len && in[i+1] == '[') {
            i += 2;
            while (i < len && in[i] != ']') i++;
            if (i < len) i++;
            if (i < len && in[i] == '(') {
                i++;
                while (i < len && in[i] != ')') i++;
                if (i < len) i++;
            }
            at_line_start = 0;
            continue;
        }
        /* Link: [text](url) -> text */
        if (in[i] == '[') {
            i++;
            while (i < len && in[i] != ']') {
                out[oi++] = in[i++];
            }
            if (i < len) i++;
            if (i < len && in[i] == '(') {
                i++;
                while (i < len && in[i] != ')') i++;
                if (i < len) i++;
            }
            at_line_start = 0;
            continue;
        }
        /* HTML tag: <...> */
        if (in[i] == '<') {
            i++;
            while (i < len && in[i] != '>') i++;
            if (i < len) i++;
            at_line_start = 0;
            continue;
        }
        /* Bold/italic markers: collapse runs of *, _ */
        if (in[i] == '*' || in[i] == '_') {
            char c = in[i];
            while (i < len && in[i] == c) i++;
            at_line_start = 0;
            continue;
        }

        out[oi++] = in[i];
        at_line_start = (in[i] == '\n');
        i++;
    }
    out[oi] = '\0';
    *out_len = oi;
    return out;
}

/* ------------------------------------------------------ table operations */

static int has_md_ext(const char *path) {
    size_t n = strlen(path);
    return n >= 3 && path[n-3] == '.' &&
           (path[n-2] == 'm' || path[n-2] == 'M') &&
           (path[n-1] == 'd' || path[n-1] == 'D');
}

static int cmp_entry(const void *a, const void *b) {
    const entry_t *ea = (const entry_t *)a;
    const entry_t *eb = (const entry_t *)b;
    return memcmp(ea->prefix, eb->prefix, (size_t)g_n * sizeof(cp_t));
}

static int cmp_prefix(const cp_t *a, const cp_t *b) {
    return memcmp(a, b, (size_t)g_n * sizeof(cp_t));
}

/* Returns first index i where entries[i].prefix >= target.  count if none. */
static size_t lower_bound(const entry_t *entries, size_t count,
                          const cp_t *target) {
    size_t lo = 0, hi = count;
    while (lo < hi) {
        size_t mid = lo + (hi - lo) / 2;
        if (cmp_prefix(entries[mid].prefix, target) < 0)
            lo = mid + 1;
        else
            hi = mid;
    }
    return lo;
}

/* Returns first index i where entries[i].prefix > target. */
static size_t upper_bound(const entry_t *entries, size_t count,
                          const cp_t *target) {
    size_t lo = 0, hi = count;
    while (lo < hi) {
        size_t mid = lo + (hi - lo) / 2;
        if (cmp_prefix(entries[mid].prefix, target) <= 0)
            lo = mid + 1;
        else
            hi = mid;
    }
    return lo;
}

/* ----------------------------------------------------------- generation */

static cp_t *generate(const entry_t *entries, size_t entry_count,
                      const cp_t *cps, size_t cp_count,
                      int n, size_t target,
                      const cp_t *initial, size_t initial_len,
                      size_t *out_count) {
    cp_t *out = malloc((target + (size_t)n) * sizeof(cp_t));
    if (!out) return NULL;
    size_t oi = 0;

    for (size_t k = 0; k < initial_len && oi < target; k++)
        out[oi++] = initial[k];

    cp_t window[32];
    if ((size_t)n > sizeof(window)/sizeof(window[0])) { free(out); return NULL; }

    if (initial_len >= (size_t)n) {
        memcpy(window, initial + (initial_len - (size_t)n),
               (size_t)n * sizeof(cp_t));
    } else if (initial_len > 0) {
        memcpy(window, initial, initial_len * sizeof(cp_t));
        size_t pad = (size_t)n - initial_len;
        if (cp_count >= (size_t)n) {
            memmove(window + pad, window, initial_len * sizeof(cp_t));
            memcpy(window, cps, pad * sizeof(cp_t));
        }
    } else {
        memcpy(window, cps, (size_t)n * sizeof(cp_t));
    }

    while (oi < target) {
        size_t lo = lower_bound(entries, entry_count, window);
        size_t hi = (lo < entry_count) ? upper_bound(entries, entry_count, window) : lo;
        if (lo == hi) {
            if (entry_count == 0) break;
            size_t r = (size_t)rand() % entry_count;
            memcpy(window, entries[r].prefix, (size_t)n * sizeof(cp_t));
            continue;
        }
        size_t pick = lo + (size_t)rand() % (hi - lo);
        cp_t next = entries[pick].next;
        out[oi++] = next;
        memmove(window, window + 1, (size_t)(n - 1) * sizeof(cp_t));
        window[n - 1] = next;
    }

    *out_count = oi;
    return out;
}

/* ------------------------------------------------------- output writing */

static int write_codepoints(FILE *fp, const cp_t *cps, size_t count) {
    char buf[4];
    for (size_t i = 0; i < count; i++) {
        int len = utf8_encode_one(cps[i], buf);
        if (fwrite(buf, 1, (size_t)len, fp) != (size_t)len) return -1;
    }
    return 0;
}

/* --------------------------------------------------------------- usage */

static void usage(const char *prog) {
    fprintf(stderr,
        "Usage: %s [OPTIONS] FILE [FILE ...]\n\n"
        "Character-level Markov chain text generator (n-gram travesty).\n\n"
        "Options:\n"
        "  -n, --ngram N      n-gram context length (1-20, default 5)\n"
        "  -c, --count N      characters to generate (default 1000)\n"
        "  -s, --start TEXT   initial text (>= n chars)\n"
        "  -o, --output FILE  output file (default stdout)\n"
        "      --seed N       random seed (default time-based)\n"
        "  -h, --help         this help\n",
        prog);
}

/* ----------------------------------------------------------------- main */

int main(int argc, char **argv) {
    int    n          = 5;
    long   count      = 1000;
    long   seed       = 0;
    int    seed_set   = 0;
    char  *start_text = NULL;
    char  *out_path   = NULL;

    static struct option lopts[] = {
        {"ngram",  required_argument, 0, 'n'},
        {"count",  required_argument, 0, 'c'},
        {"start",  required_argument, 0, 's'},
        {"output", required_argument, 0, 'o'},
        {"seed",   required_argument, 0,  1 },
        {"help",   no_argument,       0, 'h'},
        {0, 0, 0, 0}
    };

    int c;
    while ((c = getopt_long(argc, argv, "n:c:s:o:h", lopts, NULL)) != -1) {
        switch (c) {
        case 'n': n          = atoi(optarg); break;
        case 'c': count      = atol(optarg); break;
        case 's': start_text = optarg;       break;
        case 'o': out_path   = optarg;       break;
        case  1 : seed       = atol(optarg); seed_set = 1; break;
        case 'h': usage(argv[0]); return 0;
        default : usage(argv[0]); return 2;
        }
    }
    if (optind >= argc)               { usage(argv[0]); return 2; }
    if (n < 1 || n > 20)              { fprintf(stderr, "Error: --ngram must be 1..20\n"); return 2; }
    if (count < 1)                    { fprintf(stderr, "Error: --count must be > 0\n"); return 2; }

    g_n = n;
    srand(seed_set ? (unsigned)seed : (unsigned)time(NULL));

    /* --- load and concatenate inputs ----------------------------------- */
    size_t total_blen = 0;
    char  *concat     = NULL;
    for (int a = optind; a < argc; a++) {
        size_t blen = 0;
        char *raw = read_file(argv[a], &blen);
        if (!raw) {
            fprintf(stderr, "Error reading %s: %s\n", argv[a], strerror(errno));
            free(concat);
            return 1;
        }
        char *processed = raw;
        size_t plen = blen;
        if (has_md_ext(argv[a])) {
            char *stripped = strip_markdown(raw, blen, &plen);
            free(raw);
            if (!stripped) { free(concat); return 1; }
            processed = stripped;
        }
        char *grown = realloc(concat, total_blen + plen + 2);
        if (!grown) { free(processed); free(concat); return 1; }
        concat = grown;
        if (total_blen > 0) concat[total_blen++] = '\n';
        memcpy(concat + total_blen, processed, plen);
        total_blen += plen;
        concat[total_blen] = '\0';
        free(processed);
    }

    /* --- decode UTF-8 -------------------------------------------------- */
    size_t cp_count = 0;
    cp_t *cps = utf8_decode_all(concat, total_blen, &cp_count);
    free(concat);
    if (!cps) { fprintf(stderr, "Error: out of memory\n"); return 1; }
    if (cp_count <= (size_t)n) {
        fprintf(stderr, "Error: input too short (%zu chars) for n=%d\n", cp_count, n);
        free(cps);
        return 1;
    }

    /* --- decode --start to codepoints ---------------------------------- */
    cp_t *start_cps     = NULL;
    size_t start_cp_len = 0;
    if (start_text) {
        start_cps = utf8_decode_all(start_text, strlen(start_text), &start_cp_len);
        if (!start_cps) { free(cps); return 1; }
        if (start_cp_len < (size_t)n) {
            fprintf(stderr, "Error: --start is shorter than n=%d (%zu chars)\n",
                    n, start_cp_len);
            free(start_cps); free(cps);
            return 2;
        }
    }

    /* --- build n-gram table -------------------------------------------- */
    size_t entry_count = cp_count - (size_t)n;
    entry_t *entries = malloc(entry_count * sizeof(entry_t));
    if (!entries) { free(start_cps); free(cps); return 1; }
    for (size_t i = 0; i < entry_count; i++) {
        entries[i].prefix = cps + i;
        entries[i].next   = cps[i + (size_t)n];
    }
    qsort(entries, entry_count, sizeof(entry_t), cmp_entry);

    /* --- warn if --start trailing context is unknown ------------------- */
    if (start_cps) {
        const cp_t *tail = start_cps + (start_cp_len - (size_t)n);
        size_t lo = lower_bound(entries, entry_count, tail);
        if (lo == entry_count || cmp_prefix(entries[lo].prefix, tail) != 0) {
            fprintf(stderr,
                "Warning: trailing context of --start not found in corpus; "
                "continuing with random context after the initial text.\n");
        }
    }

    /* --- generate ------------------------------------------------------ */
    const cp_t *initial   = start_cps ? start_cps : cps;
    size_t      init_len  = start_cps ? start_cp_len : (size_t)n;
    size_t      out_count = 0;
    cp_t *out = generate(entries, entry_count, cps, cp_count, n, (size_t)count,
                         initial, init_len, &out_count);
    if (!out) {
        fprintf(stderr, "Error: generation failed\n");
        free(entries); free(start_cps); free(cps);
        return 1;
    }

    /* --- write output -------------------------------------------------- */
    int rc = 0;
    FILE *fp = stdout;
    if (out_path) {
        fp = fopen(out_path, "wb");
        if (!fp) {
            fprintf(stderr, "Error opening %s: %s\n", out_path, strerror(errno));
            rc = 1;
        }
    }
    if (rc == 0 && write_codepoints(fp, out, out_count) != 0) {
        fprintf(stderr, "Error writing output\n");
        rc = 1;
    }
    if (out_path && fp) fclose(fp);
    if (!out_path && out_count > 0 && out[out_count - 1] != '\n') fputc('\n', stdout);

    free(out);
    free(entries);
    free(start_cps);
    free(cps);
    return rc;
}
