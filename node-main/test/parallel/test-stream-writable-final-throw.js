'use strict';

const common = require('../common');
const {
  Duplex,
} = require('stream');

{
  class Foo extends Duplex {
    _final(callback) {
      throw new Error('fhqwhgads');
    }

    _read() {}
  }

  const foo = new Foo();
  foo._write = common.mustCall((chunk, encoding, cb) => {
    cb();
  });
  foo.end('test', common.expectsError({ message: 'fhqwhgads' }));
  foo.on('error', common.mustCall());
}
