'use strict';

// This tests NODE_COMPILE_CACHE_PACKED=1: one pack file per cache directory.

require('../common');
const { spawnSyncAndAssert } = require('../common/compile-cache');
const assert = require('assert');
const fixtures = require('../common/fixtures');
const tmpdir = require('../common/tmpdir');
const fs = require('fs');
const path = require('path');

function env(dir) {
  return {
    ...process.env,
    NODE_DEBUG_NATIVE: 'COMPILE_CACHE',
    NODE_COMPILE_CACHE: dir,
    NODE_COMPILE_CACHE_PACKED: '1',
  };
}

function packOf(dir, allowLoose = false) {
  const sub = fs.readdirSync(dir);
  assert.strictEqual(sub.length, 1);
  const entries = fs.readdirSync(path.join(dir, sub[0]));
  if (allowLoose) {
    assert.ok(entries.includes('pack'), entries.join());
  } else {
    assert.deepStrictEqual(entries, ['pack']);
  }
  return path.join(dir, sub[0], 'pack');
}

{
  // First run writes one pack file; the second run reads from it and does not rewrite it.
  tmpdir.refresh();
  const dir = tmpdir.resolve('.compile_cache_dir');
  const script = fixtures.path('snapshot', 'typescript.js');

  spawnSyncAndAssert(process.execPath, [script], { env: env(dir), cwd: tmpdir.path }, {
    stderr(output) {
      assert.match(output, /looking up .*typescript\.js in pack\.\.\. not found/);
      assert.match(output, /wrote pack .*: \d+ entries/);
      return true;
    },
  });
  const pack = packOf(dir);
  const first = fs.readFileSync(pack);
  assert.strictEqual(first.readUInt32LE(0), 0x8adfdbb3);

  spawnSyncAndAssert(process.execPath, [script], { env: env(dir), cwd: tmpdir.path }, {
    stderr(output) {
      assert.match(output, /looking up .*typescript\.js in pack\.\.\. success/);
      assert.match(output, /cache for .*typescript\.js was accepted/);
      assert.match(output, /pack unchanged, not writing/);
      return true;
    },
  });
  assert.ok(fs.readFileSync(pack).equals(first), 'the second run must not rewrite the pack');

  // A truncated pack is ignored and rewritten.
  fs.writeFileSync(pack, first.subarray(0, first.length - 1));
  spawnSyncAndAssert(process.execPath, [script], { env: env(dir), cwd: tmpdir.path }, {
    stderr(output) {
      assert.match(output, /pack .* is truncated/);
      assert.match(output, /wrote pack/);
      return true;
    },
  });
  const rewritten = fs.readFileSync(pack);
  assert.strictEqual(rewritten.readUInt32LE(0), 0x8adfdbb3);
  assert.strictEqual(rewritten.readUInt32LE(4), first.readUInt32LE(4));
}

{
  // A child process that writes the pack while the parent runs: the parent
  // merges with the pack on disk, so both keep their entries after one run.
  tmpdir.refresh();
  const dir = tmpdir.resolve('.compile_cache_dir');
  fs.writeFileSync(tmpdir.resolve('a.js'), 'module.exports = function a() { return 1; };\n');
  fs.writeFileSync(tmpdir.resolve('b.js'), 'module.exports = function b() { return 2; };\n');
  fs.writeFileSync(tmpdir.resolve('child.js'), "require('./b');\n");
  fs.writeFileSync(tmpdir.resolve('parent.js'),
                   "require('./a');\n" +
                   "const { spawnSync } = require('child_process');\n" +
                   "const r = spawnSync(process.execPath, [require('path').join(__dirname, 'child.js')]," +
                   " { stdio: 'inherit' });\n" +
                   'process.exitCode = r.status;\n');

  spawnSyncAndAssert(process.execPath, ['parent.js'], { env: env(dir), cwd: tmpdir.path }, {
    stderr: /wrote pack/,
  });
  packOf(dir);
  spawnSyncAndAssert(process.execPath, ['parent.js'], { env: env(dir), cwd: tmpdir.path }, {
    stderr(output) {
      assert.match(output, /looking up .*a\.js in pack\.\.\. success/);
      assert.match(output, /looking up .*b\.js in pack\.\.\. success/);
      assert.match(output, /looking up .*parent\.js in pack\.\.\. success/);
      assert.match(output, /looking up .*child\.js in pack\.\.\. success/);
      assert.doesNotMatch(output, /wrote pack/);
      return true;
    },
  });

  // An edited module: its new cache goes to a loose file, the pack is kept,
  // and the next run reads the module from the loose file.
  fs.writeFileSync(tmpdir.resolve('b.js'), 'module.exports = function b() { return 3; };\n');
  spawnSyncAndAssert(process.execPath, ['parent.js'], { env: env(dir), cwd: tmpdir.path }, {
    stderr(output) {
      assert.match(output, /looking up .*b\.js in pack\.\.\. code size or hash mismatch/);
      assert.match(output, /1 changed and 0 loose entries, writing loose/);
      assert.match(output, /writing cache for .*b\.js.*success/);
      assert.doesNotMatch(output, /wrote pack/);
      return true;
    },
  });
  spawnSyncAndAssert(process.execPath, ['parent.js'], { env: env(dir), cwd: tmpdir.path }, {
    stderr(output) {
      assert.match(output, /reading cache from .*for CommonJS .*b\.js\.\.\..*success/);
      assert.match(output, /looking up .*a\.js in pack\.\.\. success/);
      assert.doesNotMatch(output, /wrote pack|writing cache for/);
      return true;
    },
  });
}

{
  // Many changed entries are folded into a new pack, and the loose files
  // read on the way are removed.
  tmpdir.refresh();
  const dir = tmpdir.resolve('.compile_cache_dir');
  const N = 20;
  const write = (v) => {
    for (let i = 0; i < N; i++) {
      fs.writeFileSync(tmpdir.resolve(`m${i}.js`), `module.exports = ${v} + ${i};\n`);
    }
  };
  fs.writeFileSync(tmpdir.resolve('main.js'),
                   `for (let i = 0; i < ${N}; i++) require('./m' + i);\n`);
  write(1);
  spawnSyncAndAssert(process.execPath, ['main.js'], { env: env(dir), cwd: tmpdir.path }, {
    stderr: /wrote pack .*: 21 entries/,
  });
  // One edited module: loose.
  fs.writeFileSync(tmpdir.resolve('m0.js'), 'module.exports = 100;\n');
  spawnSyncAndAssert(process.execPath, ['main.js'], { env: env(dir), cwd: tmpdir.path }, {
    stderr: /writing loose/,
  });
  const pack = packOf(dir, true);
  assert.strictEqual(fs.readdirSync(path.dirname(pack)).length, 2);
  // 16 more edited (m1..m16) + m0 read from its loose file >= 16: fold, and
  // m0's loose file is removed.
  for (let i = 1; i <= 16; i++) {
    fs.writeFileSync(tmpdir.resolve(`m${i}.js`), `module.exports = 2 + ${i};\n`);
  }
  spawnSyncAndAssert(process.execPath, ['main.js'], { env: env(dir), cwd: tmpdir.path }, {
    stderr: /wrote pack .*: 21 entries/,
  });
  packOf(dir);
  spawnSyncAndAssert(process.execPath, ['main.js'], { env: env(dir), cwd: tmpdir.path }, {
    stderr(output) {
      assert.match(output, /looking up .*m0\.js in pack\.\.\. success/);
      assert.match(output, /pack unchanged, not writing/);
      return true;
    },
  });
}

{
  // `node -e` scripts share one cache key: alternating two of them must not
  // rewrite the pack on every run.
  tmpdir.refresh();
  const dir = tmpdir.resolve('.compile_cache_dir');
  const script = fixtures.path('snapshot', 'typescript.js');
  const evalA = `require(${JSON.stringify(script)})`;
  for (const code of [evalA, '0', evalA, '0']) {
    spawnSyncAndAssert(process.execPath, ['-e', code], { env: env(dir), cwd: tmpdir.path }, {});
  }
  for (const code of [evalA, '0']) {
    spawnSyncAndAssert(process.execPath, ['-e', code], { env: env(dir), cwd: tmpdir.path }, {
      stderr(output) {
        assert.doesNotMatch(output, /wrote pack/);
        return true;
      },
    });
  }
}

{
  // A read-only cache reads the pack and does not write to it.
  tmpdir.refresh();
  const dir = tmpdir.resolve('.compile_cache_dir');
  const script = fixtures.path('snapshot', 'typescript.js');
  spawnSyncAndAssert(process.execPath, [script], { env: env(dir), cwd: tmpdir.path }, {
    stderr: /wrote pack/,
  });
  const pack = packOf(dir);
  const before = fs.readFileSync(pack);
  const readOnlyEnv = { ...env(dir), NODE_COMPILE_CACHE_READONLY: '1' };
  spawnSyncAndAssert(process.execPath, [script], { env: readOnlyEnv, cwd: tmpdir.path }, {
    stderr(output) {
      assert.match(output, /looking up .*typescript\.js in pack\.\.\. success/);
      assert.match(output, /read-only, skipping persistence/);
      return true;
    },
  });
  packOf(dir);
  assert.ok(fs.readFileSync(pack).equals(before));
}

{
  // Without the variable, the per-file layout is unchanged.
  tmpdir.refresh();
  const dir = tmpdir.resolve('.compile_cache_dir');
  const e = env(dir);
  delete e.NODE_COMPILE_CACHE_PACKED;
  spawnSyncAndAssert(process.execPath, [fixtures.path('snapshot', 'typescript.js')],
                     { env: e, cwd: tmpdir.path }, { stderr: /writing cache for .*typescript\.js.*success/ });
  const sub = fs.readdirSync(dir);
  assert.notDeepStrictEqual(fs.readdirSync(path.join(dir, sub[0])), ['pack']);
}
