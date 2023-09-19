'use strict';

const common = require('../common');
const assert = require('assert');
const { Readable, Duplex } = require('stream');

{
  const readable = new Readable({
    read() {
    }
  });
  assert.strictEqual(readable.readableAborted, false);
  readable.destroy();
  assert.strictEqual(readable.readableAborted, true);
}

{
  const readable = new Readable({
    read() {
    }
  });
  assert.strictEqual(readable.readableAborted, false);
  readable.push(null);
  readable.destroy();
  assert.strictEqual(readable.readableAborted, true);
}

{
  const readable = new Readable({
    read() {
    }
  });
  assert.strictEqual(readable.readableAborted, false);
  readable.push('asd');
  readable.destroy();
  assert.strictEqual(readable.readableAborted, true);
}

{
  const readable = new Readable({
    read() {
    }
  });
  assert.strictEqual(readable.readableAborted, false);
  readable.push('asd');
  readable.push(null);
  assert.strictEqual(readable.readableAborted, false);
  readable.on('end', common.mustCall(() => {
    assert.strictEqual(readable.readableAborted, false);
    readable.destroy();
    assert.strictEqual(readable.readableAborted, false);
    queueMicrotask(() => {
      assert.strictEqual(readable.readableAborted, false);
    });
  }));
  readable.resume();
}

{
  const duplex = new Duplex({
    readable: false,
    write() {}
  });
  duplex.destroy();
  assert.strictEqual(duplex.readableAborted, false);
}
