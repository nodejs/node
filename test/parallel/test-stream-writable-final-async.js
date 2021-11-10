'use strict';

const common = require('../common');
const {
  Duplex,
} = require('stream');
const { setTimeout } = require('timers/promises');
const assert = require('assert');

{
  class Foo extends Duplex {
    async _final(callback) {
      // eslint-disable-next-line no-restricted-syntax
      await setTimeout(common.platformTimeout(1));
      callback();
    }

    _write = common.mustCall((chunk, encoding, cb) => {
      cb();
    })

    _read() {}
  }

  const foo = new Foo();
  foo.end('test', common.mustCall());
  foo.on('error', common.mustNotCall());
}
