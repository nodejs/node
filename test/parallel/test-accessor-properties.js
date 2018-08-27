'use strict';

require('../common');

// This tests that the accessor properties do not raise assertions
// when called with incompatible receivers.

const assert = require('assert');

// Objects that call StreamBase::AddMethods, when setting up
// their prototype
const TTY = process.binding('tty_wrap').TTY;
const UDP = process.binding('udp_wrap').UDP;

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

  // Should not throw for Object.getOwnPropertyDescriptor
  assert.strictEqual(
    typeof Object.getOwnPropertyDescriptor(TTY.prototype, 'bytesRead'),
    'object'
  );

  assert.strictEqual(
    typeof Object.getOwnPropertyDescriptor(TTY.prototype, 'fd'),
    'object'
  );

  assert.strictEqual(
    typeof Object.getOwnPropertyDescriptor(TTY.prototype, '_externalStream'),
    'object'
  );

  assert.strictEqual(
    typeof Object.getOwnPropertyDescriptor(UDP.prototype, 'fd'),
    'object'
  );
}
