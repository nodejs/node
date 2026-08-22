'use strict';
const assert = require('node:assert');
const { spawn, spawnSync } = require('node:child_process');
const { once } = require('node:events');
const { test } = require('node:test');
const { text } = require('node:stream/consumers');
const {
  testCreatePipe,
  withCreatePipe,
} = require('../common/net-create-pipe');

const holdOpen = `
  process.send('ready');
  process.on('message', (message) => {
    if (message === 'close') process.exit(0);
  });
`;

const holdStdinOpen = `
  process.stdin.resume();
  process.send('ready');
`;

function trySpawn(...args) {
  try {
    return { child: spawn(...args) };
  } catch (error) {
    return { error };
  }
}

testCreatePipe('spawn failure leaves parent-owned endpoints usable',
  async (readable, writable) => {
    const child = spawn('program-that-had-better-not-exist', [], {
      stdio: [readable, 'ignore', 'ignore'],
    });

    const close = new Promise((resolve) => child.on('close', resolve));
    const [[err]] = await Promise.all([
      once(child, 'error'),
      close,
    ]);
    assert.strictEqual(err.code, 'ENOENT');
    assert.strictEqual(readable.destroyed, false);
    assert.strictEqual(writable.destroyed, false);

    const output = text(readable);
    writable.end('abc');
    assert.strictEqual(await output, 'abc');
  });

testCreatePipe('spawnSync rejects parent-owned pipe streams',
  (readable, writable) => {
    assert.throws(() => {
      spawnSync(process.execPath, ['-e', ''], {
        stdio: [readable, 'ignore', 'ignore'],
      });
    }, {
      code: 'ERR_INVALID_ARG_VALUE',
      message: /parent-owned endpoint streams are only supported by spawn\(\)/,
    });

    readable.resume();
    writable.end();
  });

testCreatePipe('writable endpoint is rejected as child stdin',
  (readable, writable) => {
    assert.throws(() => {
      spawn(process.execPath, ['-e', ''], {
        stdio: [writable, 'ignore', 'ignore'],
      });
    }, {
      code: 'ERR_INVALID_ARG_VALUE',
    });

    readable.resume();
    writable.end();
  });

testCreatePipe('readable endpoint is rejected as child stdout',
  (readable, writable) => {
    assert.throws(() => {
      spawn(process.execPath, ['-e', ''], {
        stdio: ['ignore', readable, 'ignore'],
      });
    }, {
      code: 'ERR_INVALID_ARG_VALUE',
    });

    readable.resume();
    writable.end();
  });

testCreatePipe('readable endpoint is rejected as child stderr',
  (readable, writable) => {
    assert.throws(() => {
      spawn(process.execPath, ['-e', ''], {
        stdio: ['ignore', 'ignore', readable],
      });
    }, {
      code: 'ERR_INVALID_ARG_VALUE',
    });

    readable.resume();
    writable.end();
  });

testCreatePipe('readable cannot be leased by two children concurrently',
  async (readable, writable) => {
    const child = spawn(process.execPath, ['-e', holdStdinOpen], {
      stdio: [readable, 'ignore', 'inherit', 'ipc'],
    });
    await once(child, 'message');

    const result = trySpawn(process.execPath, ['-e', holdStdinOpen], {
      stdio: [readable, 'ignore', 'inherit', 'ipc'],
    });

    const childClose = once(child, 'close');
    const resultClose = result.child != null ? once(result.child, 'close') :
      null;
    if (result.child != null)
      result.child.kill();

    writable.end();
    await childClose;
    if (resultClose != null)
      await resultClose;
    readable.resume();

    assert.strictEqual(result.error?.code, 'ERR_INVALID_STATE');
  });

testCreatePipe('readable cannot be leased twice by the same child',
  (readable, writable) => {
    assert.throws(() => {
      spawn(process.execPath, ['-e', ''], {
        stdio: [readable, 'ignore', 'inherit', readable],
      });
    }, {
      code: 'ERR_INVALID_STATE',
    });

    readable.resume();
    writable.end();
  });

testCreatePipe('writable cannot be leased by two children concurrently',
  async (readable, writable) => {
    const child = spawn(process.execPath, ['-e', holdOpen], {
      stdio: ['ignore', writable, 'inherit', 'ipc'],
    });
    await once(child, 'message');

    const result = trySpawn(process.execPath, ['-e', holdOpen], {
      stdio: ['ignore', writable, 'inherit', 'ipc'],
    });

    const childClose = once(child, 'close');
    const resultClose = result.child != null ? once(result.child, 'close') :
      null;
    if (result.child != null)
      result.child.kill();

    child.send('close');
    await childClose;
    if (resultClose != null)
      await resultClose;
    readable.resume();
    writable.end();

    assert.strictEqual(result.error?.code, 'ERR_INVALID_STATE');
  });

testCreatePipe('writable cannot be leased twice by the same child',
  (readable, writable) => {
    assert.throws(() => {
      spawn(process.execPath, ['-e', ''], {
        stdio: ['ignore', writable, 'inherit', writable],
      });
    }, {
      code: 'ERR_INVALID_STATE',
    });

    readable.resume();
    writable.end();
  });

test('failed lease releases earlier streams from the same attempt',
  async () => {
    await withCreatePipe(async (busy) => {
      await withCreatePipe(async (free) => {
        const childA = spawn(process.execPath, ['-e', holdStdinOpen], {
          stdio: [busy.readable, 'ignore', 'inherit', 'ipc'],
        });
        await once(childA, 'message');

        assert.throws(() => {
          spawn(process.execPath, ['-e', holdStdinOpen], {
            stdio: [free.readable, 'ignore', 'inherit', busy.readable],
          });
        }, {
          code: 'ERR_INVALID_STATE',
        });

        const childC = spawn(process.execPath, ['-e', holdStdinOpen], {
          stdio: [free.readable, 'ignore', 'inherit', 'ipc'],
        });
        await once(childC, 'message');

        const childAClose = once(childA, 'close');
        const childCClose = once(childC, 'close');
        busy.writable.end();
        free.writable.end();
        await childAClose;
        await childCClose;
        busy.readable.resume();
        free.readable.resume();
      });
    });
  });

testCreatePipe('flowing readable cannot be leased', (readable, writable) => {
  readable.resume();

  assert.throws(() => {
    spawn(process.execPath, ['-e', holdStdinOpen], {
      stdio: [readable, 'ignore', 'inherit'],
    });
  }, {
    code: 'ERR_INVALID_STATE',
    message: /Readable stream must not be flowing/,
  });

  writable.end();
});
