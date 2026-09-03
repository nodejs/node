'use strict';
const common = require('../common');
const tmpdir = require('../common/tmpdir');
const assert = require('node:assert');
const fs = require('node:fs');
const fsp = require('node:fs/promises');
const path = require('node:path');
const { pathToFileURL } = require('node:url');
const test = require('node:test');
const data = 'foo';
let cnt = 0;

function nextFile() {
  return tmpdir.resolve(`${cnt++}.out`);
}

// No temp file should be left behind.
function assertNoLeftovers() {
  const leftovers = fs.readdirSync(tmpdir.path)
    .filter((entry) => entry.endsWith('.tmp'));
  assert.deepStrictEqual(leftovers, []);
}

tmpdir.refresh();

test('synchronous version', async (t) => {
  await t.test('validation', (t) => {
    for (const v of ['true', '', 0, 1, [], {}, Symbol()]) {
      assert.throws(() => {
        fs.writeFileSync(nextFile(), data, { atomic: v });
      }, { code: 'ERR_INVALID_ARG_TYPE' });
    }
  });

  await t.test('rejects a file descriptor', (t) => {
    const file = nextFile();
    const fd = fs.openSync(file, 'w');
    try {
      assert.throws(() => {
        fs.writeFileSync(fd, data, { atomic: true });
      }, { code: 'ERR_INVALID_ARG_TYPE' });
    } finally {
      fs.closeSync(fd);
    }
  });

  await t.test('rejects a non-default flag', (t) => {
    assert.throws(() => {
      fs.writeFileSync(nextFile(), data, { atomic: true, flag: 'a' });
    }, { code: 'ERR_INCOMPATIBLE_OPTION_PAIR' });
  });

  await t.test('writes via a temporary file and renames', (t) => {
    const renameSpy = t.mock.method(fs, 'renameSync');
    const file = nextFile();
    fs.writeFileSync(file, data, { atomic: true });

    const calls = renameSpy.mock.calls;
    assert.strictEqual(calls.length, 1);
    const [from, to] = calls[0].arguments;
    assert.strictEqual(to, file);
    assert.strictEqual(path.dirname(from), path.dirname(file));
    assert.strictEqual(fs.readFileSync(file, 'utf8'), data);
    assertNoLeftovers();
  });

  await t.test('flushes before renaming', (t) => {
    const spy = t.mock.method(fs, 'fsyncSync');
    fs.writeFileSync(nextFile(), data, { atomic: true });
    assert.strictEqual(spy.mock.calls.length, 1);
  });

  await t.test('does not stage a temporary file when not atomic', (t) => {
    const spy = t.mock.method(fs, 'renameSync');
    const file = nextFile();
    fs.writeFileSync(file, data);
    assert.strictEqual(spy.mock.calls.length, 0);
    assert.strictEqual(fs.readFileSync(file, 'utf8'), data);
  });

  await t.test('leaves the original intact when the write fails', (t) => {
    const file = nextFile();
    fs.writeFileSync(file, 'original');
    t.mock.method(fs, 'writeSync', () => {
      throw new Error('boom');
    });

    assert.throws(() => {
      fs.writeFileSync(file, data, { atomic: true });
    }, { message: 'boom' });
    t.mock.restoreAll();

    assert.strictEqual(fs.readFileSync(file, 'utf8'), 'original');
    assertNoLeftovers();
  });

  await t.test('leaves the original intact when the rename fails', (t) => {
    const file = nextFile();
    fs.writeFileSync(file, 'original');
    t.mock.method(fs, 'renameSync', () => {
      throw new Error('boom');
    });

    assert.throws(() => {
      fs.writeFileSync(file, data, { atomic: true });
    }, { message: 'boom' });
    t.mock.restoreAll();

    assert.strictEqual(fs.readFileSync(file, 'utf8'), 'original');
    assertNoLeftovers();
  });

  await t.test('accepts a file URL', (t) => {
    const file = nextFile();
    fs.writeFileSync(pathToFileURL(file), data, { atomic: true });
    assert.strictEqual(fs.readFileSync(file, 'utf8'), data);
    assertNoLeftovers();
  });

  await t.test('accepts a Buffer path', (t) => {
    const file = nextFile();
    fs.writeFileSync(Buffer.from(file), data, { atomic: true });
    assert.strictEqual(fs.readFileSync(file, 'utf8'), data);
    assertNoLeftovers();
  });

  await t.test('preserves the permissions of an existing file', (t) => {
    if (common.isWindows) {
      return;
    }
    const file = nextFile();
    fs.writeFileSync(file, 'original', { mode: 0o600 });
    fs.writeFileSync(file, data, { atomic: true });
    assert.strictEqual(fs.statSync(file).mode & 0o777, 0o600);
  });

  await t.test('honors mode when the file does not exist', (t) => {
    if (common.isWindows) {
      return;
    }
    const file = nextFile();
    fs.writeFileSync(file, data, { atomic: true, mode: 0o600 });
    assert.strictEqual(fs.statSync(file).mode & 0o777, 0o600);
  });
});

test('callback version', async (t) => {
  await t.test('validation', (t) => {
    for (const v of ['true', '', 0, 1, [], {}, Symbol()]) {
      assert.throws(() => {
        fs.writeFile(nextFile(), data, { atomic: v }, common.mustNotCall());
      }, { code: 'ERR_INVALID_ARG_TYPE' });
    }
  });

  await t.test('writes via a temporary file and renames', (t, done) => {
    const spy = t.mock.method(fs, 'rename');
    const file = nextFile();
    fs.writeFile(file, data, { atomic: true }, common.mustSucceed(() => {
      assert.strictEqual(spy.mock.calls.length, 1);
      assert.strictEqual(spy.mock.calls[0].arguments[1], file);
      assert.strictEqual(fs.readFileSync(file, 'utf8'), data);
      assertNoLeftovers();
      done();
    }));
  });

  await t.test('leaves the original intact when the rename fails', (t, done) => {
    const file = nextFile();
    fs.writeFileSync(file, 'original');
    t.mock.method(fs, 'rename', (from, to, cb) => {
      cb(new Error('boom'));
    });

    fs.writeFile(file, data, { atomic: true }, common.mustCall((err) => {
      assert.strictEqual(err.message, 'boom');
      t.mock.restoreAll();
      assert.strictEqual(fs.readFileSync(file, 'utf8'), 'original');
      assertNoLeftovers();
      done();
    }));
  });
});

test('promises version', async (t) => {
  await t.test('validation', async (t) => {
    for (const v of ['true', '', 0, 1, [], {}, Symbol()]) {
      await assert.rejects(
        fsp.writeFile(nextFile(), data, { atomic: v }),
        { code: 'ERR_INVALID_ARG_TYPE' },
      );
    }
  });

  await t.test('rejects a FileHandle', async (t) => {
    const handle = await fsp.open(nextFile(), 'w');
    try {
      await assert.rejects(
        fsp.writeFile(handle, data, { atomic: true }),
        { code: 'ERR_INVALID_ARG_TYPE' },
      );
    } finally {
      await handle.close();
    }
  });

  await t.test('writes via a temporary file and renames', async (t) => {
    const file = nextFile();
    await fsp.writeFile(file, data, { atomic: true });
    assert.strictEqual(await fsp.readFile(file, 'utf8'), data);
    assertNoLeftovers();
  });

  await t.test('overwrites an existing file', async (t) => {
    const file = nextFile();
    await fsp.writeFile(file, 'original');
    await fsp.writeFile(file, data, { atomic: true });
    assert.strictEqual(await fsp.readFile(file, 'utf8'), data);
    assertNoLeftovers();
  });

  await t.test('accepts a file URL', async (t) => {
    const file = nextFile();
    await fsp.writeFile(pathToFileURL(file), data, { atomic: true });
    assert.strictEqual(await fsp.readFile(file, 'utf8'), data);
    assertNoLeftovers();
  });

  await t.test('a reader never observes a partial write', async (t) => {
    const file = nextFile();
    const big = 'x'.repeat(1024 * 1024);
    await fsp.writeFile(file, big);

    const writer = fsp.writeFile(file, 'y'.repeat(1024 * 1024), { atomic: true });
    for (let i = 0; i < 20; i++) {
      const seen = await fsp.readFile(file, 'utf8');
      assert.strictEqual(seen.length, big.length);
      assert.match(seen, /^(x+|y+)$/);
    }
    await writer;
    assertNoLeftovers();
  });
});
