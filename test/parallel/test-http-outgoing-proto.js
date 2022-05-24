'use strict';
require('../common');
const assert = require('assert');

const http = require('http');
const OutgoingMessage = http.OutgoingMessage;
const ClientRequest = http.ClientRequest;
const ServerResponse = http.ServerResponse;

assert.strictEqual(
  typeof ClientRequest.prototype._implicitHeader, 'function');
assert.strictEqual(
  typeof ServerResponse.prototype._implicitHeader, 'function');

// validateHeader
assert.throws(() => {
  const outgoingMessage = new OutgoingMessage();
  outgoingMessage.setHeader();
}, {
  code: 'ERR_INVALID_HTTP_TOKEN',
  name: 'TypeError',
});

assert.throws(() => {
  const outgoingMessage = new OutgoingMessage();
  outgoingMessage.setHeader('test');
}, {
  code: 'ERR_HTTP_INVALID_HEADER_VALUE',
  name: 'TypeError',
});

assert.throws(() => {
  const outgoingMessage = new OutgoingMessage();
  outgoingMessage.setHeader(404);
}, {
  code: 'ERR_INVALID_HTTP_TOKEN',
  name: 'TypeError',
});

assert.throws(() => {
  const outgoingMessage = new OutgoingMessage();
  outgoingMessage.setHeader.call({ _header: 'test' }, 'test', 'value');
}, {
  code: 'ERR_HTTP_HEADERS_SENT',
  name: 'Error',
});

assert.throws(() => {
  const outgoingMessage = new OutgoingMessage();
  outgoingMessage.setHeader('200', 'あ');
}, {
  code: 'ERR_INVALID_CHAR',
  name: 'TypeError',
});

// write
{
  const outgoingMessage = new OutgoingMessage();

  assert.throws(
    () => {
      outgoingMessage.write('');
    },
    {
      code: 'ERR_METHOD_NOT_IMPLEMENTED',
      name: 'Error',
    }
  );
}

assert.throws(() => {
  const outgoingMessage = new OutgoingMessage();
  outgoingMessage.write.call({ _header: 'test', _hasBody: 'test' });
}, {
  code: 'ERR_INVALID_ARG_TYPE',
  name: 'TypeError',
});

assert.throws(() => {
  const outgoingMessage = new OutgoingMessage();
  outgoingMessage.write.call({ _header: 'test', _hasBody: 'test' }, 1);
}, {
  code: 'ERR_INVALID_ARG_TYPE',
  name: 'TypeError',
});

assert.throws(() => {
  const outgoingMessage = new OutgoingMessage();
  outgoingMessage.write.call({ _header: 'test', _hasBody: 'test' }, null);
}, {
  code: 'ERR_STREAM_NULL_VALUES',
  name: 'TypeError'
});

// addTrailers()
// The `Error` comes from the JavaScript engine so confirm that it is a
// `TypeError` but do not check the message. It will be different in different
// JavaScript engines.
assert.throws(() => {
  const outgoingMessage = new OutgoingMessage();
  outgoingMessage.addTrailers();
}, TypeError);

assert.throws(() => {
  const outgoingMessage = new OutgoingMessage();
  outgoingMessage.addTrailers({ 'あ': 'value' });
}, {
  code: 'ERR_INVALID_HTTP_TOKEN',
  name: 'TypeError',
});

assert.throws(() => {
  const outgoingMessage = new OutgoingMessage();
  outgoingMessage.addTrailers({ 404: 'あ' });
}, {
  code: 'ERR_INVALID_CHAR',
  name: 'TypeError',
});

{
  const outgoingMessage = new OutgoingMessage();
  assert.strictEqual(outgoingMessage.destroyed, false);
  outgoingMessage.destroy();
  assert.strictEqual(outgoingMessage.destroyed, true);
}
