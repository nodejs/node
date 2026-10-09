'use strict';

// Regression test for https://github.com/nodejs/node/issues/61958
//
// fs.rmSync() and fsPromises.rm() are documented as the synchronous and
// asynchronous forms of one API, but they do not share an implementation:
// rmSync() dispatches to binding.rmSync() (std::filesystem::remove_all) while
// rm() and fsPromises.rm() use the JS rimraf in lib/internal/fs/rimraf.js.
// They disagree for paths whose trailing component is `.` or `..`: the sync
// form removes the directory contents and reports success, while the async
// form rejects with EINVAL and removes nothing.
//
// Each case asserts two things. First the invariant from the bug report: for
// identical input the two forms must succeed or fail the same way and leave
// the filesystem in the same state. Second, that the surviving tree is the one
// POSIX path resolution implies, so that the test still fails if both forms
// are wrong in the same way.

const common = require('../common');
const tmpdir = require('../common/tmpdir');
const assert = require('node:assert');
const fs = require('node:fs');
const fsPromises = require('node:fs/promises');
const path = require('node:path');
const { pathToFileURL } = require('node:url');

tmpdir.refresh();

const RM_OPTIONS = { recursive: true, force: true };

// Path shapes whose trailing component is `.` or `..`, plus controls that
// should be unaffected. Kept as raw strings: path.join() would normalize the
// dot segments away and destroy the case under test.
const DOT_SEGMENT_PATHS = [
  'a/b/../.',      // The shape reported in the issue.
  'a/b/..',
  'a/b/.././b',
  'a/.',
  'a/b/c/.',
  'a/b/c/../..',
  'a/b/c/d/../../..',
  './a',           // Control: leading `.` only.
  'a/b',           // Control: no dot segments at all.
];

// The fixture tree, in the shape listTree() reports it.
const FIXTURE_ENTRIES = ['a', 'a/b', 'a/b/c', 'a/b/c/d'];

// What must still exist afterwards: every entry that is neither the resolved
// target nor below it. Derived from POSIX path resolution rather than from what
// either implementation happens to do, so that agreeing on a wrong answer still
// fails the test.
function expectedSurvivors(relative) {
  const target = path.posix.normalize(relative);
  return FIXTURE_ENTRIES.filter(
    (entry) => entry !== target && !entry.startsWith(`${target}/`));
}

let fixtureCounter = 0;

// Builds <tmpdir>/rm-fixture-N/a/b/c/d and returns the fixture root.
function makeFixture() {
  const root = tmpdir.resolve(`rm-fixture-${fixtureCounter++}`);
  fs.mkdirSync(path.join(root, 'a', 'b', 'c', 'd'), { recursive: true });
  return root;
}

// Sorted, root-relative listing of everything under `root`, with `/` as the
// separator so two runs can be compared directly on any platform.
function listTree(root) {
  const entries = [];
  (function walk(dir) {
    for (const entry of fs.readdirSync(dir, { withFileTypes: true })) {
      const absolute = path.join(dir, entry.name);
      entries.push(path.relative(root, absolute).split(path.sep).join('/'));
      if (entry.isDirectory()) walk(absolute);
    }
  })(root);
  return entries.sort();
}

// Joins by hand rather than with path.join() to preserve dot segments.
function targetPath(root, relative, encoding) {
  const raw = `${root}/${relative}`;
  if (encoding === 'string') return raw;
  if (encoding === 'buffer') return Buffer.from(raw);
  if (encoding === 'url') return pathToFileURL(raw);
  assert.fail(`unknown encoding ${encoding}`);
}

// Normalizes an outcome to something comparable across the two code paths.
function settled(fn) {
  try {
    fn();
    return { outcome: 'success' };
  } catch (err) {
    return { outcome: 'failure', code: err.code };
  }
}

async function settledAsync(fn) {
  try {
    await fn();
    return { outcome: 'success' };
  } catch (err) {
    return { outcome: 'failure', code: err.code };
  }
}

// Runs the sync and async forms against separate but identical fixtures and
// reports what each did.
async function compareForms(relative, encoding) {
  const syncRoot = makeFixture();
  const syncResult = settled(
    () => fs.rmSync(targetPath(syncRoot, relative, encoding), RM_OPTIONS));
  const syncTree = listTree(syncRoot);

  const asyncRoot = makeFixture();
  const asyncResult = await settledAsync(
    () => fsPromises.rm(targetPath(asyncRoot, relative, encoding), RM_OPTIONS));
  const asyncTree = listTree(asyncRoot);

  return { syncResult, syncTree, asyncResult, asyncTree };
}

async function assertFormsAgree(relative, encoding) {
  const label = `rm('${relative}') with a ${encoding} path`;
  const { syncResult, syncTree, asyncResult, asyncTree } =
    await compareForms(relative, encoding);

  assert.deepStrictEqual(
    asyncResult, syncResult,
    `${label}: fsPromises.rm() and fs.rmSync() disagree. ` +
    `sync=${JSON.stringify(syncResult)} async=${JSON.stringify(asyncResult)}`);

  assert.deepStrictEqual(
    asyncTree, syncTree,
    `${label}: fsPromises.rm() and fs.rmSync() left different trees behind. ` +
    `sync=${JSON.stringify(syncTree)} async=${JSON.stringify(asyncTree)}`);

  const expected = expectedSurvivors(relative);

  assert.deepStrictEqual(
    syncResult, { outcome: 'success' },
    `${label}: fs.rmSync() should remove the resolved target, but reported ` +
    JSON.stringify(syncResult));

  assert.deepStrictEqual(
    syncTree, expected,
    `${label}: fs.rmSync() left the wrong tree. ` +
    `got=${JSON.stringify(syncTree)} want=${JSON.stringify(expected)}`);

  assert.deepStrictEqual(
    asyncTree, expected,
    `${label}: fsPromises.rm() left the wrong tree. ` +
    `got=${JSON.stringify(asyncTree)} want=${JSON.stringify(expected)}`);
}

// A path whose bytes are not valid UTF-8. Normalizing a Buffer path through a
// UTF-8 round trip rewrites these bytes to U+FFFD, which would resolve to a
// different path than the caller asked for -- silently, in an API that deletes.
//
// Filenames are arbitrary bytes on most POSIX systems, but not everywhere:
// Windows names are UTF-16, and macOS rejects invalid UTF-8 outright. Rather
// than enumerate platforms, this asks the filesystem and skips if it will not
// store the name.
async function assertNonUtf8BufferPathsSurvive() {
  const oddName = Buffer.from([0xff, 0xfe]);

  for (const form of ['sync', 'async']) {
    // A bare directory, not makeFixture(): this case only needs the oddly named
    // subtree, and anything else in the root would just be noise here.
    const root = tmpdir.resolve(`rm-nonutf8-${fixtureCounter++}`);
    fs.mkdirSync(root, { recursive: true });
    const oddDir = Buffer.concat([Buffer.from(`${root}/`), oddName]);
    try {
      fs.mkdirSync(oddDir);
    } catch (err) {
      // The filesystem will not store these bytes as a name, so there is
      // nothing to assert about removing them here.
      if (err.code === 'EILSEQ' || err.code === 'EINVAL') {
        return;
      }
      throw err;
    }
    fs.mkdirSync(Buffer.concat([oddDir, Buffer.from('/child')]));

    // Resolves to oddDir itself, so the whole directory should be removed.
    const target = Buffer.concat([oddDir, Buffer.from('/child/..')]);

    if (form === 'sync') {
      fs.rmSync(target, RM_OPTIONS);
    } else {
      await fsPromises.rm(target, RM_OPTIONS);
    }

    assert.strictEqual(
      fs.existsSync(oddDir), false,
      `fs.rm (${form}) left a directory with non-UTF-8 bytes in its name behind; ` +
      'the path was probably rewritten during normalization');
    assert.deepStrictEqual(
      listTree(root), [],
      `fs.rm (${form}) left something behind under the fixture root`);
  }
}

(async () => {
  // String paths: the form reported in the issue.
  for (const relative of DOT_SEGMENT_PATHS) {
    await assertFormsAgree(relative, 'string');
  }

  // Buffer paths. fs.rm() documents `string | Buffer | URL`, so the dot
  // segment handling must not depend on how the path was supplied. This is
  // the case the reviewer asked about on PR #61968 and that was never
  // answered.
  for (const relative of DOT_SEGMENT_PATHS) {
    await assertFormsAgree(relative, 'buffer');
  }

  // URL paths. The WHATWG URL parser resolves dot segments itself, so both
  // forms should receive an already-normalized path here; this pins that
  // assumption so a future change to path handling cannot silently break it.
  for (const relative of DOT_SEGMENT_PATHS) {
    await assertFormsAgree(relative, 'url');
  }

  await assertNonUtf8BufferPathsSurvive();
})().then(common.mustCall());
