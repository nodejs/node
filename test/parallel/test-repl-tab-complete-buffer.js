'use strict';

const common = require('../common');
const ArrayStream = require('../common/arraystream');
const { hijackStderr, restoreStderr } = require('../common/hijackstdio');
const assert = require('assert');

const repl = require('repl');

const input = new ArrayStream();
const replServer = repl.start({
  prompt: '',
  input,
  output: process.stdout,
  allowBlockingCompletions: true,
});

// Some errors are passed to the domain, but do not callback
replServer._domain.on('error', assert.ifError);

for (const type of [
  Array,
  Buffer,

  Uint8Array,
  Uint16Array,
  Uint32Array,

  Uint8ClampedArray,
  Int8Array,
  Int16Array,
  Int32Array,
  Float32Array,
  Float64Array,
]) {
  input.run(['.clear']);

  if (type === Array) {
    input.run([
      'var ele = [];',
      'for (let i = 0; i < 1e6 + 1; i++) ele[i] = 0;',
      'ele.biu = 1;',
    ]);
  } else if (type === Buffer) {
    input.run(['var ele = Buffer.alloc(1e6 + 1); ele.biu = 1;']);
  } else {
    input.run([`var ele = new ${type.name}(1e6 + 1); ele.biu = 1;`]);
  }

  hijackStderr(common.mustNotCall());
  replServer.complete(
    'ele.',
    common.mustCall((err, data) => {
      restoreStderr();
      assert.ifError(err);

      const ele =
        type === Array ? [] : type === Buffer ? Buffer.alloc(0) : new type(0);

      assert.strictEqual(data[0].includes('ele.biu'), true);

      data[0].forEach((key) => {
        if (!key || key === 'ele.biu') return;
        assert.notStrictEqual(ele[key.slice(4)], undefined);
      });
    })
  );
}

// check Buffer.prototype.length not crashing.
// Refs: https://github.com/nodejs/node/pull/11961
input.run(['.clear']);
replServer.complete('Buffer.prototype.', common.mustCall());
