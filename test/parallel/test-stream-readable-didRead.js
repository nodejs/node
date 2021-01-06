'use strict';
require('../common');
const assert = require('assert');
const Readable = require('stream').Readable;

{
  const readable = new Readable({
    read: () => {}
  });

  assert.strictEqual(readable.readableDidRead, false);
  readable.read();
  assert.strictEqual(readable.readableDidRead, true);
}

{
  const readable = new Readable({
    read: () => {}
  });

  assert.strictEqual(readable.readableDidRead, false);
  readable.resume();
  assert.strictEqual(readable.readableDidRead, true);
}
