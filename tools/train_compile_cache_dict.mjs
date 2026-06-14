#!/usr/bin/env node
// Regenerates src/compile_cache_zstd.dict, the zstd dictionary embedded in the
// binary and used to compress compile-cache entries (see src/compile_cache.cc).
//
// The dictionary is trained on V8 code caches harvested via vm.compileFunction
// (the same shape the CJS loader produces) from a fixed in-tree corpus, then
// fed to `zstd --train`. The shipped src/compile_cache_zstd.dict is the source
// of truth; this script documents and reproduces how it was made.
//
// Reproducibility — the output is byte-for-byte stable only if all of the
// following are pinned:
//
//   * node is run with --predictable. V8 otherwise randomizes the string hash
//     seed per process, and that seed leaks into vm.compileFunction cachedData,
//     so every harvest (and therefore every trained dictionary) would differ
//     run-to-run even on the same machine. This script re-executes itself with
//     --predictable if needed.
//   * the corpus and its order are fixed (CORPUS below, walked sorted).
//   * the node build is fixed: cachedData embeds the V8 version and build
//     flags, so a different node produces a different (still valid) dictionary.
//   * the zstd CLI matches deps/zstd (currently 1.5.7). ZDICT training defaults
//     change between zstd releases; this script refuses to run on a mismatch.
//
// Training on --predictable caches is fine even though the runtime consumes
// non-predictable caches: the dictionary only supplies shared substrings, and
// Persist() keeps min(plain, dict) per entry, so a less-than-ideal dictionary
// can never make any entry larger.
//
// Usage:  node tools/train_compile_cache_dict.mjs

import { spawnSync } from 'node:child_process';
import { readFileSync, writeFileSync, mkdtempSync, mkdirSync, readdirSync,
         rmSync } from 'node:fs';
import { join, relative, dirname } from 'node:path';
import { tmpdir } from 'node:os';
import { fileURLToPath } from 'node:url';

const ROOT = join(dirname(fileURLToPath(import.meta.url)), '..');
const OUT = join(ROOT, 'src', 'compile_cache_zstd.dict');
const MAXDICT = 16384;
const REQUIRED_ZSTD = '1.5.7';      // keep in sync with deps/zstd/lib/zstd.h

// Fixed corpus, relative to the repo root. Chosen to be diverse, always
// present in a checkout, and disjoint from the held-out measurement corpora
// (e.g. test/parallel) used to report size numbers.
const CORPUS = ['lib', 'tools', 'deps/npm/node_modules'];

// V8 randomizes the hash seed per process; re-exec under --predictable so the
// harvested caches — and the trained dictionary — are deterministic.
if (!process.execArgv.includes('--predictable')) {
  const r = spawnSync(process.execPath,
    ['--predictable', fileURLToPath(import.meta.url), ...process.argv.slice(2)],
    { stdio: 'inherit' });
  process.exit(r.status ?? 1);
}

function checkZstd() {
  const r = spawnSync('zstd', ['--version'], { encoding: 'utf8' });
  if (r.status !== 0) {
    console.error('error: zstd CLI not found on PATH'); process.exit(1);
  }
  const m = r.stdout.match(/v(\d+\.\d+\.\d+)/);
  if (!m || m[1] !== REQUIRED_ZSTD) {
    console.error(`error: zstd ${REQUIRED_ZSTD} required (matching deps/zstd), ` +
                  `found ${m ? m[1] : 'unknown'}`);
    process.exit(1);
  }
}

const PARAMS = ['exports', 'require', 'module', '__filename', '__dirname'];

function* walk(dir) {
  let ents;
  try { ents = readdirSync(dir, { withFileTypes: true }); } catch { return; }
  for (const e of ents.sort((a, b) => (a.name < b.name ? -1 : 1))) {
    const p = join(dir, e.name);
    if (e.isDirectory()) yield* walk(p);
    else if (e.isFile() && p.endsWith('.js')) yield p;
  }
}

async function main() {
  checkZstd();
  const { default: vm } = await import('node:vm');

  const files = [];
  for (const root of CORPUS) for (const f of walk(join(ROOT, root))) files.push(f);
  files.sort();

  const samples = mkdtempSync(join(tmpdir(), 'cc-dict-'));
  mkdirSync(samples, { recursive: true });
  const sampleFiles = [];
  let ok = 0;
  for (const f of files) {
    let code;
    try { code = readFileSync(f, 'utf8'); } catch { continue; }
    try {
      const fn = vm.compileFunction(code, PARAMS,
        { filename: f, produceCachedData: true });
      const cd = fn.cachedData;
      if (cd && cd.length > 0) {
        const name = relative(ROOT, f).replace(/[\\/]/g, '_') + '.cache';
        const out = join(samples, name);
        writeFileSync(out, cd);
        sampleFiles.push(out);
        ok++;
      }
    } catch { /* skip modules V8 can't compile standalone */ }
  }
  sampleFiles.sort();
  console.error(`harvested ${ok}/${files.length} code caches from ` +
                `${CORPUS.join(', ')}`);

  const r = spawnSync('zstd',
    ['--train', ...sampleFiles, `--maxdict=${MAXDICT}`, '-f', '-o', OUT],
    { stdio: ['ignore', 'ignore', 'inherit'] });
  rmSync(samples, { recursive: true, force: true });
  if (r.status !== 0) { console.error('error: zstd --train failed'); process.exit(1); }

  const size = readFileSync(OUT).length;
  console.error(`wrote ${relative(ROOT, OUT)} (${size} bytes)`);
}

main();
