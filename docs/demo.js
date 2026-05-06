// monkey-typewriter — browser demo
// Char- and word-level Markov chain text generator with temperature.

(() => {
  'use strict';

  const $ = (id) => document.getElementById(id);
  const els = {
    input: $('input'),
    inputStats: $('input-stats'),
    mode: $('mode'),
    n: $('n'),
    nOut: $('n-out'),
    count: $('count'),
    countOut: $('count-out'),
    temp: $('temp'),
    tempOut: $('temp-out'),
    go: $('go'),
    output: $('output'),
    viz: $('viz'),
    buildStats: $('build-stats'),
  };

  // -------------------------------------------------------------- markov

  function tokenise(text, mode) {
    if (mode === 'word') {
      return text.match(/\w+|[^\w\s]/gu) || [];
    }
    // character mode — array of code points (handles surrogate pairs)
    return Array.from(text);
  }

  function buildTable(units, n) {
    const sep = (typeof units[0] === 'string' && units[0].length > 1) ? ' ' : '';
    const wordMode = sep === ' ';
    const table = new Map();
    for (let i = 0; i < units.length - n; i++) {
      const prefix = wordMode
        ? units.slice(i, i + n).join(' ')
        : units.slice(i, i + n).join('');
      const next = units[i + n];
      let followers = table.get(prefix);
      if (!followers) { followers = new Map(); table.set(prefix, followers); }
      followers.set(next, (followers.get(next) || 0) + 1);
    }
    return { table, wordMode };
  }

  function sampleWithTemp(followers, T) {
    if (T <= 0) {
      let best, bestC = -1;
      for (const [k, v] of followers) {
        if (v > bestC) { best = k; bestC = v; }
      }
      return best;
    }
    const items = [...followers.entries()];
    const total = items.reduce((s, [, c]) => s + c, 0);
    const weights = items.map(([, c]) => Math.pow(c / total, 1 / T));
    const sumW = weights.reduce((s, w) => s + w, 0);
    let r = Math.random() * sumW;
    for (let i = 0; i < items.length; i++) {
      r -= weights[i];
      if (r <= 0) return items[i][0];
    }
    return items[items.length - 1][0];
  }

  function generate(units, n, count, T, table, wordMode) {
    const sep = wordMode ? ' ' : '';
    const initial = units.slice(0, n);
    let prefix = initial.join(sep);
    let output = wordMode ? initial.join(' ') : prefix;
    let produced = n;
    let keys = null;

    while (produced < count) {
      let followers = table.get(prefix);
      if (!followers || followers.size === 0) {
        if (keys === null) keys = [...table.keys()];
        if (keys.length === 0) break;
        prefix = keys[Math.floor(Math.random() * keys.length)];
        continue;
      }
      const next = sampleWithTemp(followers, T);
      if (wordMode) {
        const isPunct = next.length === 1 && /[^\w\s]/u.test(next);
        output += (isPunct ? '' : ' ') + next;
        const tokens = prefix.split(' ').slice(1);
        tokens.push(next);
        prefix = tokens.join(' ');
      } else {
        output += next;
        prefix = prefix.slice(1) + next;
      }
      produced++;
    }
    return output;
  }

  // -------------------------------------------------------- visualisation

  function renderViz(table, wordMode, topPrefixes = 12, topFollowers = 6) {
    els.viz.innerHTML = '';
    const sorted = [...table.entries()]
      .map(([prefix, followers]) => {
        let total = 0;
        for (const v of followers.values()) total += v;
        return [prefix, followers, total];
      })
      .sort((a, b) => b[2] - a[2])
      .slice(0, topPrefixes);

    const fmtUnit = (s) => {
      if (s === '\n') return '\\n';
      if (s === '\t') return '\\t';
      if (s === ' ')  return '·';
      return s;
    };
    const fmtPrefix = (p) => {
      if (wordMode) {
        return p.split(' ').map(fmtUnit).join(' ');
      }
      return [...p].map(fmtUnit).join('');
    };

    for (const [prefix, followers, total] of sorted) {
      const block = document.createElement('div');
      block.className = 'viz-prefix';

      const head = document.createElement('div');
      head.className = 'viz-prefix-head';
      const lab = document.createElement('span');
      lab.className = 'label';
      lab.textContent = `"${fmtPrefix(prefix)}"`;
      const tot = document.createElement('span');
      tot.className = 'total';
      tot.textContent = `${total.toLocaleString()} occurrences`;
      head.appendChild(lab);
      head.appendChild(tot);
      block.appendChild(head);

      const items = [...followers.entries()].sort((a, b) => b[1] - a[1]);
      for (const [follower, cnt] of items.slice(0, topFollowers)) {
        const pct = 100 * cnt / total;
        const row = document.createElement('div');
        row.className = 'viz-row';

        const g = document.createElement('span');
        g.className = 'glyph';
        g.textContent = fmtUnit(follower);
        const p = document.createElement('span');
        p.className = 'pct';
        p.textContent = pct.toFixed(1) + '%';
        const barWrap = document.createElement('span');
        const bar = document.createElement('div');
        bar.className = 'viz-bar';
        bar.style.width = pct + '%';
        barWrap.appendChild(bar);

        row.appendChild(g);
        row.appendChild(p);
        row.appendChild(barWrap);
        block.appendChild(row);
      }
      if (items.length > topFollowers) {
        const more = document.createElement('div');
        more.style.color = 'var(--muted)';
        more.style.fontSize = '0.75rem';
        more.style.marginTop = '0.3rem';
        more.textContent = `… and ${items.length - topFollowers} more followers`;
        block.appendChild(more);
      }
      els.viz.appendChild(block);
    }
  }

  // ------------------------------------------------------------------ UI

  function updateOutputs() {
    els.nOut.textContent = els.n.value;
    els.countOut.textContent = els.count.value;
    els.tempOut.textContent = parseFloat(els.temp.value).toFixed(1);
  }
  els.n.addEventListener('input', updateOutputs);
  els.count.addEventListener('input', updateOutputs);
  els.temp.addEventListener('input', updateOutputs);
  updateOutputs();

  els.input.addEventListener('input', () => {
    const length = els.input.value.length;
    els.inputStats.textContent = `${length.toLocaleString()} characters loaded`;
  });

  async function loadSample() {
    try {
      const response = await fetch('sample.txt');
      if (!response.ok) throw new Error(response.statusText);
      const text = await response.text();
      els.input.value = text;
      els.inputStats.textContent = `${text.length.toLocaleString()} characters loaded (Goethe — Faust I)`;
    } catch (err) {
      els.input.value = 'Could not load sample text. Paste any text here and click Generate.';
      els.inputStats.textContent = 'Sample failed to load: ' + err.message;
    }
  }
  loadSample();

  els.go.addEventListener('click', () => {
    const text = els.input.value;
    const mode = els.mode.value;
    const n = parseInt(els.n.value, 10);
    const count = parseInt(els.count.value, 10);
    const T = parseFloat(els.temp.value);

    if (text.length < n + 1) {
      els.output.textContent = 'Input is too short for this n. Paste more text or lower n.';
      return;
    }

    els.go.disabled = true;
    els.output.textContent = 'Building table…';
    els.buildStats.textContent = '';
    els.viz.innerHTML = '';

    setTimeout(() => {
      try {
        const t0 = performance.now();
        const units = tokenise(text, mode);
        if (units.length <= n) {
          els.output.textContent = `Tokenised input has only ${units.length} units, need > n=${n}.`;
          return;
        }
        const { table, wordMode } = buildTable(units, n);
        const t1 = performance.now();
        const out = generate(units, n, count, T, table, wordMode);
        const t2 = performance.now();
        els.output.textContent = out;
        renderViz(table, wordMode);
        els.buildStats.textContent =
          `${table.size.toLocaleString()} prefixes · build ${(t1 - t0).toFixed(0)} ms · ` +
          `generate ${(t2 - t1).toFixed(0)} ms`;
      } catch (err) {
        els.output.textContent = 'Error: ' + err.message;
      } finally {
        els.go.disabled = false;
      }
    }, 10);
  });
})();
