'use strict';

const common = require('../common');
const assert = require('assert');
const { MessageChannel, receiveMessageOnPort } = require('worker_threads');

// Regression test for https://github.com/nodejs/node/issues/67766

{
  // receiveMessageOnPort() on a port that was just transferred must not crash.
  const { port1 } = new MessageChannel();
  const other = new MessageChannel();
  other.port1.postMessage(null, [port1]);
  assert.strictEqual(receiveMessageOnPort(port1), undefined);
  other.port1.close();
}

{
  // structuredClone() of a port whose close has completed must throw a
  // DataCloneError instead of crashing.
  const { port1 } = new MessageChannel();
  port1.close(common.mustCall(() => {
    setImmediate(common.mustCall(() => {
      assert.throws(() => structuredClone(port1), {
        name: 'DataCloneError',
        message: 'Cannot clone object of unsupported type.',
      });
    }));
  }));
}
