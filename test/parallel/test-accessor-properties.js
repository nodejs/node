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
    UDP.prototype.fd; // eslint-disable-line no-unused-expressions
  }, TypeError);

  const StreamWrapProto = Object.getPrototypeOf(TTY.prototype);
  const properties = ['bytesRead', 'fd', '_externalStream'];

  properties.forEach((property) => {
    // Should throw instead of raise assertions
    assert.throws(() => {
      TTY.prototype[property]; // eslint-disable-line no-unused-expressions
    }, TypeError, `Missing expected TypeError for TTY.prototype.${property}`);

    // Should not throw for Object.getOwnPropertyDescriptor
    assert.strictEqual(
      typeof Object.getOwnPropertyDescriptor(StreamWrapProto, property),
      'object',
      'typeof property descriptor ' + property + ' is not \'object\''
    );
  });

  if (common.hasCrypto) { // eslint-disable-line node-core/crypto-check
    // There are accessor properties in crypto too
    const crypto = internalBinding('crypto');

    assert.throws(() => {
      // eslint-disable-next-line no-unused-expressions
      crypto.SecureContext.prototype._external;
    }, TypeError);

    assert.strictEqual(
      typeof Object.getOwnPropertyDescriptor(
        crypto.SecureContext.prototype, '_external'),
      'object'
    );
  }
}
