'use strict';

const common = require('../common');
const assert = require('assert');
const {
  Duplex,
} = require('stream');

{
  let written = '';

  const duplex = new Duplex({
    allowHalfOpen: true,
    read() {
      this.push('hello');
      this.push(null);
    },
    write(chunk, encoding, callback) {
      written += chunk;
      callback();
    },
  });

  duplex.on('error', common.mustNotCall());
  duplex.on('close', common.mustCall());

  (async () => {
    let read = '';
    for await (const chunk of duplex) {
      read += chunk;
    }

    assert.strictEqual(read, 'hello');
    assert.strictEqual(duplex.destroyed, false);

    duplex.write('world', common.mustSucceed(() => {
      assert.strictEqual(written, 'world');
      duplex.end();
    }));
  })().then(common.mustCall());
}
