'use strict';

const common = require('../common');
const {
  Duplex,
  Writable,
  Transform,
} = require('stream');
const { setTimeout } = require('timers/promises');
const assert = require('assert');

{
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
}

{
  class Foo extends Duplex {
    async _destroy(err, cb) {
      // eslint-disable-next-line no-restricted-syntax
      await setTimeout(common.platformTimeout(1));
      throw new Error('boom');
    }
  }

  const foo = new Foo();
  foo.destroy();
  foo.on('error', common.expectsError({
    message: 'boom'
  }));
  foo.on('close', common.mustCall(() => {
    assert(foo.destroyed);
  }));
}

{
  class Foo extends Duplex {
    async _destroy(err, cb) {
      // eslint-disable-next-line no-restricted-syntax
      await setTimeout(common.platformTimeout(1));
    }
  }

  const foo = new Foo();
  foo.destroy();
  foo.on('close', common.mustCall(() => {
    assert(foo.destroyed);
  }));
}

{
  class Foo extends Duplex {
    async _construct() {
      // eslint-disable-next-line no-restricted-syntax
      await setTimeout(common.platformTimeout(1));
    }

    _write = common.mustCall((chunk, encoding, cb) => {
      cb();
    })

    _read() {}
  }

  const foo = new Foo();
  foo.write('test', common.mustCall());
}

{
  class Foo extends Duplex {
    async _construct(callback) {
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
  foo.write('test', common.mustCall());
  foo.on('error', common.expectsError({
    code: 'ERR_MULTIPLE_CALLBACK'
  }));
}

{
  class Foo extends Writable {
    _write = common.mustCall((chunk, encoding, cb) => {
      cb();
    })

    async _final() {
      // eslint-disable-next-line no-restricted-syntax
      await setTimeout(common.platformTimeout(1));
    }
  }

  const foo = new Foo();
  foo.end('hello');
  foo.on('finish', common.mustCall());
}

{
  class Foo extends Writable {
    _write = common.mustCall((chunk, encoding, cb) => {
      cb();
    })

    async _final(callback) {
      // eslint-disable-next-line no-restricted-syntax
      await setTimeout(common.platformTimeout(1));
      callback();
    }
  }

  const foo = new Foo();
  foo.end('hello');
  foo.on('finish', common.mustCall());
}

{
  class Foo extends Writable {
    _write = common.mustCall((chunk, encoding, cb) => {
      cb();
    })

    async _final() {
      // eslint-disable-next-line no-restricted-syntax
      await setTimeout(common.platformTimeout(1));
      throw new Error('boom');
    }
  }

  const foo = new Foo();
  foo.end('hello');
  foo.on('error', common.expectsError({
    message: 'boom'
  }));
  foo.on('close', common.mustCall());
}

{
  const expected = ['hello', 'world'];
  class Foo extends Transform {
    async _flush() {
      return 'world';
    }

    _transform(chunk, encoding, callback) {
      callback(null, chunk);
    }
  }

  const foo = new Foo();
  foo.end('hello');
  foo.on('data', common.mustCall((chunk) => {
    assert.strictEqual(chunk.toString(), expected.shift());
  }, 2));
}

{
  const expected = ['hello', 'world'];
  class Foo extends Transform {
    async _flush(callback) {
      callback(null, 'world');
    }

    _transform(chunk, encoding, callback) {
      callback(null, chunk);
    }
  }

  const foo = new Foo();
  foo.end('hello');
  foo.on('data', common.mustCall((chunk) => {
    assert.strictEqual(chunk.toString(), expected.shift());
  }, 2));
}

{
  class Foo extends Transform {
    async _flush(callback) {
      throw new Error('boom');
    }

    _transform(chunk, encoding, callback) {
      callback(null, chunk);
    }
  }

  const foo = new Foo();
  foo.end('hello');
  foo.on('data', common.mustCall());
  foo.on('error', common.expectsError({
    message: 'boom'
  }));
  foo.on('close', common.mustCall());
}

{
  class Foo extends Transform {
    async _transform(chunk) {
      return chunk.toString().toUpperCase();
    }
  }

  const foo = new Foo();
  foo.end('hello');
  foo.on('data', common.mustCall((chunk) => {
    assert.strictEqual(chunk.toString(), 'HELLO');
  }));
}

{
  class Foo extends Transform {
    async _transform(chunk, _, callback) {
      callback(null, chunk.toString().toUpperCase());
    }
  }

  const foo = new Foo();
  foo.end('hello');
  foo.on('data', common.mustCall((chunk) => {
    assert.strictEqual(chunk.toString(), 'HELLO');
  }));
}

{
  class Foo extends Transform {
    async _transform() {
      throw new Error('boom');
    }
  }

  const foo = new Foo();
  foo.end('hello');
  foo.on('error', common.expectsError({
    message: 'boom'
  }));
  foo.on('close', common.mustCall());
}
