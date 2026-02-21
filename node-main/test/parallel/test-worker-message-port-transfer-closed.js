'use strict';

const common = require('../common');
const assert = require('assert');
const { MessageChannel } = require('worker_threads');

// This tests various behaviors around transferring MessagePorts with closing
// or closed handles.

const { port1, port2 } = new MessageChannel();

const arrayBuf = new ArrayBuffer(10);
port1.onmessage = common.mustNotCall();
port2.onmessage = common.mustNotCall();

function testSingle(closedPort, potentiallyOpenPort) {
  assert.throws(common.mustCall(() => {
    potentiallyOpenPort.postMessage(null, [arrayBuf, closedPort]);
  }), common.mustCall((err) => {
    assert.strictEqual(err.name, 'DataCloneError');
    assert.strictEqual(err.message,
                       'MessagePort in transfer list is already detached');
    assert.strictEqual(err.code, 25);
    assert.ok(err instanceof Error);

    const DOMException = err.constructor;
    assert.ok(err instanceof DOMException);
    assert.strictEqual(DOMException.name, 'DOMException');

    return true;
  }));

  // arrayBuf must not be transferred, even though it is present earlier in the
  // transfer list than the closedPort.
  assert.strictEqual(arrayBuf.byteLength, 10);
}

function testBothClosed() {
  testSingle(port1, port2);
  testSingle(port2, port1);
}

// Even though the port handles may not be completely closed in C++ land, the
// observable behavior must be that the closing/detachment is synchronous and
// instant.

port1.close(common.mustCall(testBothClosed));
testSingle(port1, port2);
port2.close(common.mustCall(testBothClosed));
testBothClosed();

function tickUnref(n, fn) {
  if (n === 0) return fn();
  setImmediate(tickUnref, n - 1, fn).unref();
}

tickUnref(10, common.mustNotCall('The communication channel is still open'));
