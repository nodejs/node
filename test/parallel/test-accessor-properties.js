// Flags: --expose-internals
'use strict';

const common = require('../common');

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

  if (common.hasCrypto) { // eslint-disable-line node-core/crypto-check
    // There are accessor properties in crypto too
    const crypto = process.binding('crypto');

    assert.throws(() => {
      crypto.SecureContext.prototype._external;
    }, TypeError);

    assert.strictEqual(
      typeof Object.getOwnPropertyDescriptor(
        crypto.SecureContext.prototype, '_external'),
      'object'
    );
  }
}
