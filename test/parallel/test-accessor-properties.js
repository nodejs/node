'use strict';

const common = require('../common');

if (!common.hasCrypto)
  common.skip('missing crypto');
// This tests that the accessor properties do not raise assertions
// when called with incompatible receivers.

const assert = require('assert');

// Objects that call StreamBase::AddMethods, when setting up
// their prototype
const TTY = process.binding('tty_wrap').TTY;
const UDP = process.binding('udp_wrap').UDP;

// There are accessor properties in crypto too
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
