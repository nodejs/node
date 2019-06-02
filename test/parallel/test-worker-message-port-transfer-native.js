// Flags: --expose-internals
'use strict';
const common = require('../common');
const assert = require('assert');
const { MessageChannel } = require('worker_threads');
const { internalBinding } = require('internal/test/binding');

// Test that passing native objects and functions to .postMessage() throws
// DataCloneError exceptions.

{
  const { port1, port2 } = new MessageChannel();
  port2.once('message', common.mustNotCall());

  assert.throws(() => {
    port1.postMessage(function foo() {});
  }, {
    name: 'DataCloneError',
    message: /function foo\(\) \{\} could not be cloned\.$/
  });
  port1.close();
}

{
  const { port1, port2 } = new MessageChannel();
  port2.once('message', common.mustNotCall());

  const nativeObject = new (internalBinding('js_stream').JSStream)();

  assert.throws(() => {
    port1.postMessage(nativeObject);
  }, {
    name: 'DataCloneError',
    message: /Cannot transfer object of unsupported type\.$/
  });
  port1.close();
}
