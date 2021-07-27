'use strict';
const common = require('../common');
const assert = require('assert');
const Readable = require('stream').Readable;

{
  const readable = new Readable({
    read: () => {}
  });

  assert.strictEqual(readable.readableDidRead, false);
  readable.read();
  assert.strictEqual(readable.readableDidRead, false);
}

{
  const readable = new Readable({
    read: () => {}
  });

  assert.strictEqual(readable.readableDidRead, false);
  readable.resume();
  assert.strictEqual(readable.readableDidRead, false);
}

{
  const readable = new Readable({
    read: () => {}
  });
  readable.push('asd');

  assert.strictEqual(readable.readableDidRead, false);
  readable.read();
  assert.strictEqual(readable.readableDidRead, true);
}

{
  const readable = new Readable({
    read: () => {}
  });
  readable.push('asd');

  assert.strictEqual(readable.readableDidRead, false);
  readable.resume();
  assert.strictEqual(readable.readableDidRead, false);
  readable.on('data', common.mustCall(function() {
    assert.strictEqual(readable.readableDidRead, true);
  }));
}
