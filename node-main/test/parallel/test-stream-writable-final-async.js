'use strict';

const common = require('../common');
const {
  Duplex,
} = require('stream');
const { setTimeout } = require('timers/promises');

{
  class Foo extends Duplex {
    async _final(callback) {
      await setTimeout(common.platformTimeout(1));
      callback();
    }

    _read() {}
  }

  const foo = new Foo();
  foo._write = common.mustCall((chunk, encoding, cb) => {
    cb();
  });
  foo.end('test', common.mustCall());
  foo.on('error', common.mustNotCall());
}
