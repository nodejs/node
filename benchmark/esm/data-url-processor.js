// Benchmarks dataURLProcessor(), which decodes the body of `data:` URLs used
// by import() and Worker. `plain` and `base64` bodies contain no '%' and so
// take the unescaped path; `percent` bodies are percent-decoded, and
// `percent-tail` bodies have a single escape at the very end.
'use strict';

const assert = require('node:assert');
const common = require('../common.js');

const bench = common.createBenchmark(main, {
  n: [1e4],
  size: [64, 1024, 16384, 65536],
  encoding: ['plain', 'percent', 'percent-tail', 'percent-mid', 'percent-at-64', 'base64'],
}, { flags: ['--expose-internals'] });

function main({ n, size, encoding }) {
  const { dataURLProcessor } = require('internal/data_url');
  let source = `export default '${'a'.repeat(size)}';`;
  let body = source;
  let mime = 'text/javascript';
  if (encoding === 'percent') {
    body = encodeURIComponent(source);
  } else if (encoding === 'percent-tail') {
    body = `${source}%0A`;
    source += '\n';
  } else if (encoding === 'percent-mid') {
    const half = source.length >> 1;
    body = `${source.slice(0, half)}%0A${source.slice(half)}`;
    source = `${source.slice(0, half)}\n${source.slice(half)}`;
  } else if (encoding === 'percent-at-64') {
    body = `${'x'.repeat(64)}${encodeURIComponent(source)}`;
    source = `${'x'.repeat(64)}${source}`;
  } else if (encoding === 'base64') {
    body = Buffer.from(source).toString('base64');
    mime += ';base64';
  }
  const url = new URL(`data:${mime},${body}`);
  let result;

  for (let i = 0; i < 1000; i++) {
    dataURLProcessor(url);
  }

  bench.start();
  for (let i = 0; i < n; i++) {
    result = dataURLProcessor(url);
  }
  bench.end(n);

  assert.strictEqual(result.mimeType.essence, 'text/javascript');
  assert.deepStrictEqual(result.body, new TextEncoder().encode(source));
}
