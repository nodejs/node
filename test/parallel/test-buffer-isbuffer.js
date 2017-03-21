'use strict';
require('../common');
const assert = require('assert');
const { Buffer } = require('buffer');

const t = (val, exp) => assert.strictEqual(Buffer.isBuffer(val), exp);

function FakeBuffer() {}
FakeBuffer.prototype = Object.create(Buffer.prototype);

class ExtensibleBuffer extends Buffer {
  constructor() {
    super(new ArrayBuffer(0), 0, 0);
    Object.setPrototypeOf(this, new.target.prototype);
  }
}

t(0, false);
t(true, false);
t('foo', false);
t(Symbol(), false);
t(null, false);
t(undefined, false);
t(() => {}, false);

t({}, false);
t(new Uint8Array(2), false);
t(new FakeBuffer(), false);
t(new ExtensibleBuffer(), true);
t(Buffer.from('foo'), true);
t(Buffer.allocUnsafeSlow(2), true);
