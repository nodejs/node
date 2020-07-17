'use strict';

const common = require('../common');
const { Duplex } = require('stream');
const { setTimeout } = require('timers/promises');
const assert = require('assert');

class Foo extends Duplex {
  async _construct(cb) {
    // eslint-disable-next-line no-restricted-syntax
    await setTimeout(common.platformTimeout(1));
    cb();
    throw new Error('boom');
  }
}

const foo = new Foo();
foo.on('error', common.expectsError({
  message: 'boom'
}));
foo.on('close', common.mustCall(() => {
  assert(foo._writableState.constructed);
  assert(foo._readableState.constructed);
}));
