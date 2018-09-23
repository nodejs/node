// Flags: --expose-internals
'use strict';

require('../common');

// This tests that the accessor properties do not raise assertions
// when called with incompatible receivers.

const assert = require('assert');

// Objects that call StreamBase::AddMethods, when setting up
// their prototype
const { internalBinding } = require('internal/test/binding');
const TTY = internalBinding('tty_wrap').TTY;
const UDP = internalBinding('udp_wrap').UDP;

{
  // Should throw instead of raise assertions
  assert.throws(() => {
    TTY.prototype.bytesRead;
  }, TypeError);

  assert.throws(() => {
    TTY.prototype.fd;
  }, TypeError);

  assert.throws(() => {
    TTY.prototype._externalStream;
  }, TypeError);

  assert.throws(() => {
    UDP.prototype.fd;
  }, TypeError);

  const StreamWrapProto = Object.getPrototypeOf(TTY.prototype);

  // Should not throw for Object.getOwnPropertyDescriptor
  assert.strictEqual(
    typeof Object.getOwnPropertyDescriptor(StreamWrapProto, 'bytesRead'),
    'object'
  );

  assert.strictEqual(
    typeof Object.getOwnPropertyDescriptor(StreamWrapProto, 'fd'),
    'object'
  );

  assert.strictEqual(
    typeof Object.getOwnPropertyDescriptor(StreamWrapProto, '_externalStream'),
    'object'
  );

  assert.strictEqual(
    typeof Object.getOwnPropertyDescriptor(StreamWrapProto, 'fd'),
    'object'
  );
}
