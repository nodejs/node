import * as common from '../common/index.mjs';
import hijackstdio from '../common/hijackstdio.js';
import assert from 'node:assert';
import { startNewREPLServer, complete } from '../common/repl.js';

const { replServer, run } = startNewREPLServer();

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
  await run(['.clear']);

  if (type === Array) {
    await run([
      'var ele = [];',
      'for (let i = 0; i < 1e6 + 1; i++) ele[i] = 0;',
      'ele.biu = 1;',
    ]);
  } else if (type === Buffer) {
    await run(['var ele = Buffer.alloc(1e6 + 1); ele.biu = 1;']);
  } else {
    await run([`var ele = new ${type.name}(1e6 + 1); ele.biu = 1;`]);
  }

  hijackstdio.hijackStderr(common.mustNotCall());
  const data = await complete(replServer, 'ele.');
  hijackstdio.restoreStderr();

  const ele =
    type === Array ? [] : type === Buffer ? Buffer.alloc(0) : new type(0);

  assert.strictEqual(data[0].includes('ele.biu'), true);

  for (const key of data[0]) {
    if (!key || key === 'ele.biu') break;
    assert.notStrictEqual(ele[key.slice(4)], undefined);
  }
}

// check Buffer.prototype.length not crashing.
// Refs: https://github.com/nodejs/node/pull/11961
await run(['.clear']);
await complete(replServer, 'Buffer.prototype.');

replServer.close();
