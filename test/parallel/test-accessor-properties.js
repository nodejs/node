'use strict';

require('../common');

// This tests that the accessor properties do not raise assersions
// when called with incompatible receivers.

const assert = require('assert');

// Examples are things that calls StreamBase::AddMethods when setting up
// their prototype
const TTY = process.binding('tty_wrap').TTY;
const UDP = process.binding('udp_wrap').UDP;

// Or the accessor properties in crypto
const crypto = process.binding('crypto');

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

  assert.throws(() => {
    crypto.SecureContext.prototype._external;
  }, TypeError);

  assert.throws(() => {
    crypto.Connection.prototype._external;
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

  assert.strictEqual(
    typeof Object.getOwnPropertyDescriptor(
      crypto.SecureContext.prototype, '_external'),
    'object'
  );

  assert.strictEqual(
    typeof Object.getOwnPropertyDescriptor(
      crypto.Connection.prototype, '_external'),
    'object'
  );
}
