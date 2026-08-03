'use strict';

// Tests correctness of fs.mkdirSync with recursive:true for deeply nested
// paths. MKDirpSync caches the continuation_data pointer in a local variable
// to avoid repeated virtual dispatch; these tests verify the optimization does
// not alter observable behaviour across various path depths.

const common = require('../common');
const assert = require('assert');
const fs = require('fs');
const path = require('path');

const tmpdir = require('../common/tmpdir');
tmpdir.refresh();

let dirc = 0;
function nextdir() { return String(++dirc); }

// Deep path creation: all segments new, verify return value is the first
// created segment and all intermediate directories exist.
{
  const depth = 8;
  const segments = Array.from({ length: depth }, nextdir);
  const pathname = tmpdir.resolve(...segments);
  const firstCreated = tmpdir.resolve(segments[0]);

  const result = fs.mkdirSync(pathname, { recursive: true });

  assert.strictEqual(result, path.toNamespacedPath(firstCreated));
  assert.ok(fs.existsSync(pathname));
  assert.ok(fs.statSync(pathname).isDirectory());

  // Every intermediate directory must exist.
  for (let i = 1; i < depth; i++) {
    const intermediate = tmpdir.resolve(...segments.slice(0, i + 1));
    assert.ok(fs.statSync(intermediate).isDirectory(),
              `intermediate path missing: ${intermediate}`);
  }
}

// Depth 16 — forces more iterations through the continuation_data loop.
{
  const depth = 16;
  const segments = Array.from({ length: depth }, nextdir);
  const pathname = tmpdir.resolve(...segments);

  fs.mkdirSync(pathname, { recursive: true });

  assert.ok(fs.existsSync(pathname));
  assert.ok(fs.statSync(pathname).isDirectory());
}

// Idempotent: calling mkdirSync twice on an existing deep path must not throw
// and must return undefined (no new directories were created).
{
  const segments = Array.from({ length: 8 }, nextdir);
  const pathname = tmpdir.resolve(...segments);

  fs.mkdirSync(pathname, { recursive: true });
  const result = fs.mkdirSync(pathname, { recursive: true });

  assert.strictEqual(result, undefined);
  assert.ok(fs.existsSync(pathname));
}

// Partial creation: first N segments already exist — return value is the first
// newly created segment.
{
  const segments = Array.from({ length: 6 }, nextdir);
  const existingBase = tmpdir.resolve(...segments.slice(0, 3));
  fs.mkdirSync(existingBase, { recursive: true });

  const pathname = tmpdir.resolve(...segments);
  const firstNew = tmpdir.resolve(...segments.slice(0, 4));

  const result = fs.mkdirSync(pathname, { recursive: true });

  assert.strictEqual(result, path.toNamespacedPath(firstNew));
  assert.ok(fs.existsSync(pathname));
  assert.ok(fs.statSync(pathname).isDirectory());
}

// Path with ".." components: mkdirSync must still create the correct directory.
{
  const a = nextdir();
  const b = nextdir();
  const c = nextdir();
  const pathname = `${tmpdir.path}/${a}/../${b}/${c}`;
  fs.mkdirSync(pathname, { recursive: true });
  assert.ok(fs.existsSync(pathname));
  assert.ok(fs.statSync(pathname).isDirectory());
}

// Async counterpart: deep path via fs.mkdir callback.
{
  const segments = Array.from({ length: 8 }, nextdir);
  const pathname = tmpdir.resolve(...segments);
  const firstCreated = tmpdir.resolve(segments[0]);

  fs.mkdir(pathname, { recursive: true }, common.mustSucceed((result) => {
    assert.strictEqual(result, path.toNamespacedPath(firstCreated));
    assert.ok(fs.existsSync(pathname));
    assert.ok(fs.statSync(pathname).isDirectory());
  }));
}
