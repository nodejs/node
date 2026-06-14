#!/usr/bin/env node
// =============================================================================
// train_compile_cache_dict.mjs
//
// Maintainer tool that regenerates src/compile_cache_zstd.dict, the zstd
// dictionary embedded in the node binary and used to shrink compile-cache
// entries on disk (see src/compile_cache.cc).
//
// -----------------------------------------------------------------------------
// What it does
// -----------------------------------------------------------------------------
// The node compile cache stores V8 code caches (the bytecode/metadata blob V8
// produces when it compiles a module) so later runs can skip recompilation.
// These blobs share a lot of structure across files, so we zstd-compress them
// with a trained dictionary to cut the cache's on-disk footprint.
//
// This script builds that dictionary end to end:
//
//   1. Walks a fixed in-tree corpus (CORPUS below) in sorted order.
//   2. For each .js file, harvests a V8 code cache via vm.compileFunction with
//      produceCachedData — the same shape the CommonJS loader produces at
//      runtime — and writes each blob to a temp directory.
//   3. Feeds all the harvested blobs to `zstd --train`, capping the result at
//      MAXDICT (16 KiB) bytes.
//   4. Overwrites src/compile_cache_zstd.dict with the trained dictionary.
//
// The shipped src/compile_cache_zstd.dict is the source of truth; this script
// exists to document and reproduce exactly how that file was made, so a future
// maintainer can regenerate it (e.g. after a V8/corpus change) and review the
// resulting diff.
//
// -----------------------------------------------------------------------------
// Usage
// -----------------------------------------------------------------------------
//   node tools/train_compile_cache_dict.mjs
//
// Run from anywhere (paths are resolved relative to this file). The output is
// written in place to src/compile_cache_zstd.dict; inspect the diff afterwards
// and commit it if it looks right. Progress is printed to stderr.
//
// Prerequisites:
//   * the `zstd` CLI on PATH, version REQUIRED_ZSTD (matching deps/zstd).
//   * a built node on PATH (this is the node you invoke it with).
//
// Note: --predictable is added automatically (see below) — you do not need to
// pass it yourself.
//
// -----------------------------------------------------------------------------
// Reproducibility
// -----------------------------------------------------------------------------
// The output is byte-for-byte stable only if all of the following are pinned:
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
// =============================================================================

import { spawnSync } from 'node:child_process';
import { readFileSync, writeFileSync, mkdtempSync, readdirSync,
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
  const sampleFiles = [];
  let ok = 0;
  let r;
  try {
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

    // One sample path per file is spread on argv; the fixed corpus (~1.4k
    // files, a few hundred KB of paths) stays well under ARG_MAX.
    r = spawnSync('zstd',
      ['--train', ...sampleFiles, `--maxdict=${MAXDICT}`, '-f', '-o', OUT],
      { stdio: ['ignore', 'ignore', 'inherit'] });
  } finally {
    rmSync(samples, { recursive: true, force: true });
  }
  if (r.status !== 0) { console.error('error: zstd --train failed'); process.exit(1); }

  const size = readFileSync(OUT).length;
  console.error(`wrote ${relative(ROOT, OUT)} (${size} bytes)`);
}

main().catch((e) => { console.error(e); process.exit(1); });
