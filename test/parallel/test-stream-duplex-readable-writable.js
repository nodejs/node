'use strict';

const common = require('../common');
const { Duplex } = require('stream');
const assert = require('assert');

{
  const duplex = new Duplex({
    readable: false
  });
  assert.strictEqual(duplex.readable, false);
  duplex.push('asd');
  duplex.on('error', common.mustCall((err) => {
    assert.strictEqual(err.code, 'ERR_STREAM_PUSH_AFTER_EOF');
  }));
  duplex.on('data', common.mustNotCall());
  duplex.on('end', common.mustNotCall());
}

{
  const duplex = new Duplex({
    writable: false,
    write: common.mustNotCall()
  });
  assert.strictEqual(duplex.writable, false);
  duplex.write('asd');
  duplex.on('error', common.mustCall((err) => {
    assert.strictEqual(err.code, 'ERR_STREAM_WRITE_AFTER_END');
  }));
  duplex.on('finish', common.mustNotCall());
}

{
  const duplex = new Duplex({
    readable: false
  });
  assert.strictEqual(duplex.readable, false);
  duplex.on('data', common.mustNotCall());
  duplex.on('end', common.mustNotCall());
  async function run() {
    for await (const chunk of duplex) {
      assert(false, chunk);
    }
  }
  run().then(common.mustCall());
}
