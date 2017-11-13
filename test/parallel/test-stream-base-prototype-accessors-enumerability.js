'use strict';

require('../common');

// This tests that the prototype accessors added by StreamBase::AddMethods
// are not enumerable. They could be enumerated when inspecting the prototype
// with util.inspect or the inspector protocol.

const assert = require('assert');

// Or anything that calls StreamBase::AddMethods when setting up its prototype
const TTY = process.binding('tty_wrap').TTY;

{
  assert.strictEqual(TTY.prototype.propertyIsEnumerable('bytesRead'), false);
  assert.strictEqual(TTY.prototype.propertyIsEnumerable('fd'), false);
  assert.strictEqual(
    TTY.prototype.propertyIsEnumerable('_externalStream'), false);
}
