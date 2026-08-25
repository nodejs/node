'use strict';

require('../common');

const assert = require('assert');

const classesToBeTested = [ MessageChannel, MessagePort ];

classesToBeTested.forEach((cls) => {
  assert.strictEqual(cls.prototype[Symbol.toStringTag], cls.name);
  assert.deepStrictEqual(Object.getOwnPropertyDescriptor(cls.prototype, Symbol.toStringTag),
                         { configurable: true, enumerable: false, value: cls.name, writable: false });
});

const channel = new MessageChannel();
assert.strictEqual(Object.prototype.toString.call(channel), '[object MessageChannel]');
assert.strictEqual(Object.prototype.toString.call(channel.port1), '[object MessagePort]');
