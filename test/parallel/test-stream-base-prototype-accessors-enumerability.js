// Flags: --expose-internals
'use strict';

require('../common');

// This tests that the prototype accessors added by StreamBase::AddMethods
// are not enumerable. They could be enumerated when inspecting the prototype
// with util.inspect or the inspector protocol.

const assert = require('assert');

// Or anything that calls StreamBase::AddMethods when setting up its prototype
const { internalBinding } = require('internal/test/binding');
const TTY = internalBinding('tty_wrap').TTY;

{
  const ttyIsEnumerable = Object.prototype.propertyIsEnumerable.bind(TTY);
  assert.strictEqual(ttyIsEnumerable('bytesRead'), false);
  assert.strictEqual(ttyIsEnumerable('fd'), false);
  assert.strictEqual(ttyIsEnumerable('_externalStream'), false);
}
