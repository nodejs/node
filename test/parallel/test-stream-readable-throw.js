'use strict';

const common = require('../common');
const { Readable } = require('stream');
const assert = require('assert');

{
  const readable = new Readable({ autoDestroy: false });

  readable._read = () => {
    throw new Error();
  };
  readable.on('error', common.mustCall());
  readable.on('close', common.mustNotCall());
  readable.resume();
}

{
  const readable = new Readable({ autoDestroy: true });

  readable._read = () => {
    throw new Error();
  };
  readable.on('error', common.mustCall());
  readable.on('close', common.mustCall());
  readable.resume();
}

{
  const readable = new Readable({ autoDestroy: true });

  readable._read = () => {
    throw new Error();
  };
  readable.on('error', common.mustCall());
  readable.on('close', common.mustCall());
  assert.strictEqual(readable.read(), null);
}
