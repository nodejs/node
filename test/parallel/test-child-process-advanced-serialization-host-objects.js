// Flags: --expose-internals
'use strict';

// Regression tests for the native `advanced` IPC codec (see PR #63933).
// Host-object classification and tag validation must match the previous
// JavaScript (v8.DefaultSerializer-based) behavior exactly.
const common = require('../common');
const assert = require('assert');
const { fork } = require('child_process');
const v8 = require('v8');
const { MessageChannel } = require('worker_threads');
const { internalBinding } = require('internal/test/binding');

if (process.argv[2] === 'inspect') {
  process.on('message', (value) => {
    process.send({
      isBuffer: Buffer.isBuffer(value),
      keys: Object.keys(value),
      visible: value?.visible,
    });
  });
  return;
}

function inspect(value) {
  return new Promise((resolve, reject) => {
    const child = fork(__filename, ['inspect'], {
      serialization: 'advanced',
      stdio: ['ignore', 'ignore', 'inherit', 'ipc'],
    });
    child.once('message', (message) => {
      child.disconnect();
      resolve(message);
    });
    try {
      child.send(value);
    } catch (err) {
      child.disconnect();
      reject(err);
    }
  });
}

async function main() {
  // 1) Spreading a non-ArrayBufferView host object must copy own properties
  //    with [[DefineOwnProperty]] semantics, i.e. without invoking inherited
  //    setters (the JS codec used `{ ...object }`).
  {
    const { port1, port2 } = new MessageChannel();
    port1.visible = 1;
    Object.defineProperty(Object.prototype, 'visible', {
      configurable: true,
      get() { return undefined; },
      set() { throw new Error('setter called'); },
    });
    let message;
    try {
      message = await inspect(port1);
    } finally {
      delete Object.prototype.visible;
      port1.close();
      port2.close();
    }
    assert.strictEqual(message.visible, 1);
    assert.strictEqual(message.keys[0], 'visible');
  }

  // 2) A Buffer whose `.constructor` is reassigned is classified by its
  //    constructor, exactly like v8.DefaultSerializer: it is no longer a
  //    Buffer on the receiving side.
  {
    const buf = Buffer.from('abc');
    buf.constructor = Uint8Array;
    const message = await inspect(buf);
    assert.strictEqual(message.isBuffer, false);
  }

  // 3) Conversely, a Uint8Array with `.constructor === Buffer` round-trips as
  //    a Buffer.
  {
    const uint8 = new Uint8Array([1, 2, 3]);
    uint8.constructor = Buffer;
    const message = await inspect(uint8);
    assert.strictEqual(message.isBuffer, true);
  }

  // 4) Tampering with Buffer.prototype.constructor must not fool the
  //    classifier: a genuine Buffer is then classified by its (now non-Buffer)
  //    constructor, matching v8.DefaultSerializer, which compares against a
  //    stable Buffer reference rather than re-reading Buffer.prototype.
  {
    const original = Buffer.prototype.constructor;
    Buffer.prototype.constructor = Uint8Array;
    let message;
    try {
      message = await inspect(Buffer.from('abc'));
    } finally {
      Buffer.prototype.constructor = original;
    }
    assert.strictEqual(message.isBuffer, false);
  }

  // 5) A host-object tag other than the two the codec emits (0 and 1) must be
  //    rejected rather than silently accepted.
  {
    const { deserialize } = internalBinding('ipc_serdes');

    class BadTagSerializer extends v8.DefaultSerializer {
      _writeHostObject(value) {
        this.writeUint32(2); // Valid tags are only 0 or 1
        return super._writeHostObject(value);
      }
    }
    const ser = new BadTagSerializer();
    ser.writeHeader();
    ser.writeValue(Buffer.from('x'));
    const payload = ser.releaseBuffer();

    assert.throws(() => deserialize(payload), { code: 'ERR_INVALID_STATE' });
  }
}

main().then(common.mustCall());
