'use strict';

const EventEmitter = require('events');
const { Duplex } = require('stream');
const _tls_wrap = require('_tls_wrap');
const _tls_common = require('_tls_common');

const {
  ArrayIsArray,
  ArrayPrototypeForEach,
  ArrayPrototypePush,
  StringPrototypeIndexOf,
  StringPrototypeSlice,
  StringPrototypeSplit,
  ObjectCreate,
  Symbol,
  ReflectConstruct,
} = primordials;

const kCallback = Symbol('Callback');
const kOtherSide = Symbol('Other');

class DuplexSocket extends Duplex {
  constructor() {
    super();
    this[kCallback] = null;
    this[kOtherSide] = null;
  }

  _read() {
    const callback = this[kCallback];
    if (callback) {
      this[kCallback] = null;
      callback();
    }
  }

  _write(chunk, encoding, callback) {
    if (chunk.length === 0) {
      process.nextTick(callback);
    } else {
      this[kOtherSide].push(chunk);
      this[kOtherSide][kCallback] = callback;
    }
  }

  _final(callback) {
    this[kOtherSide].on('end', callback);
    this[kOtherSide].push(null);
  }
}

class DuplexPair {
  constructor() {
    this.socket1 = new DuplexSocket();
    this.socket2 = new DuplexSocket();
    this.socket1[kOtherSide] = this.socket2;
    this.socket2[kOtherSide] = this.socket1;
  }
}

class SecurePair extends EventEmitter {
  constructor(secureContext = _tls_common.createSecureContext(),
              isServer = false,
              requestCert = !isServer,
              rejectUnauthorized = false,
              options = {}) {
    super();
    const { socket1, socket2 } = new DuplexPair();

    this.server = options.server;
    this.credentials = secureContext;

    this.encrypted = socket1;
    this.cleartext = new _tls_wrap.TLSSocket(socket2, {
      secureContext,
      isServer,
      requestCert,
      rejectUnauthorized,
      ...options
    });
    this.cleartext.once('secure', () => this.emit('secure'));
  }

  destroy() {
    this.cleartext.destroy();
    this.encrypted.destroy();
  }
}

// Example:
// C=US\nST=CA\nL=SF\nO=Joyent\nOU=Node.js\nCN=ca1\nemailAddress=ry@clouds.org
function parseCertString(s) {
  const out = ObjectCreate(null);
  ArrayPrototypeForEach(StringPrototypeSplit(s, '\n'), (part) => {
    const sepIndex = StringPrototypeIndexOf(part, '=');
    if (sepIndex > 0) {
      const key = StringPrototypeSlice(part, 0, sepIndex);
      const value = StringPrototypeSlice(part, sepIndex + 1);
      if (key in out) {
        if (!ArrayIsArray(out[key])) {
          out[key] = [out[key]];
        }
        ArrayPrototypePush(out[key], value);
      } else {
        out[key] = value;
      }
    }
  });
  return out;
}

exports.parseCertString = parseCertString;

exports.createSecurePair = function createSecurePair(...args) {
  return ReflectConstruct(SecurePair, args);
};
