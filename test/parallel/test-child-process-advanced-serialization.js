'use strict';
const common = require('../common');
const assert = require('assert');
const child_process = require('child_process');
const { once } = require('events');

if (process.argv[2] !== 'child') {
  for (const value of [null, 42, Infinity, 'foo']) {
    assert.throws(() => {
      child_process.spawn(process.execPath, [], { serialization: value });
    }, {
      code: 'ERR_INVALID_OPT_VALUE',
      message: `The value "${value}" is invalid ` +
        'for option "options.serialization"'
    });
  }

  (async () => {
    const cp = child_process.spawn(process.execPath, [__filename, 'child'],
                                   {
                                     stdio: ['ipc', 'inherit', 'inherit'],
                                     serialization: 'advanced'
                                   });

    const circular = {};
    circular.circular = circular;
    for await (const message of [
      { uint8: new Uint8Array(4) },
      { float64: new Float64Array([ Math.PI ]) },
      { buffer: Buffer.from('Hello!') },
      { map: new Map([{ a: 1 }, { b: 2 }]) },
      { bigInt: 1337n },
      circular,
      new Error('Something went wrong'),
      new RangeError('Something range-y went wrong'),
    ]) {
      cp.send(message);
      const [ received ] = await once(cp, 'message');
      assert.deepStrictEqual(received, message);
    }

    cp.disconnect();
  })().then(common.mustCall());
} else {
  process.on('message', (msg) => process.send(msg));
}
