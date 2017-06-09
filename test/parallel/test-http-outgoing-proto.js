'use strict';
require('../common');
const assert = require('assert');

const http = require('http');
const OutgoingMessage = http.OutgoingMessage;
const ClientRequest = http.ClientRequest;
const ServerResponse = http.ServerResponse;

assert.throws(OutgoingMessage.prototype._implicitHeader,
              /^Error: _implicitHeader\(\) method is not implemented$/);
assert.strictEqual(
  typeof ClientRequest.prototype._implicitHeader, 'function');
assert.strictEqual(
  typeof ServerResponse.prototype._implicitHeader, 'function');

// validateHeader
assert.throws(() => {
  const outgoingMessage = new OutgoingMessage();
  outgoingMessage.setHeader();
}, /^TypeError: Header name must be a valid HTTP Token \["undefined"\]$/);

assert.throws(() => {
  const outgoingMessage = new OutgoingMessage();
  outgoingMessage.setHeader('test');
}, /^Error: "value" required in setHeader\("test", value\)$/);

assert.throws(() => {
  const outgoingMessage = new OutgoingMessage();
  outgoingMessage.setHeader(404);
}, /^TypeError: Header name must be a valid HTTP Token \["404"\]$/);

assert.throws(() => {
  const outgoingMessage = new OutgoingMessage();
  outgoingMessage.setHeader.call({_header: 'test'}, 'test', 'value');
}, /^Error: Can't set headers after they are sent\.$/);

assert.throws(() => {
  const outgoingMessage = new OutgoingMessage();
  outgoingMessage.setHeader('200', 'あ');
}, /^TypeError: The header content contains invalid characters$/);

// write
assert.throws(() => {
  const outgoingMessage = new OutgoingMessage();
  outgoingMessage.write();
}, /^Error: _implicitHeader\(\) method is not implemented$/);

assert(OutgoingMessage.prototype.write.call({_header: 'test'}));

assert.throws(() => {
  const outgoingMessage = new OutgoingMessage();
  outgoingMessage.write.call({_header: 'test', _hasBody: 'test'});
}, /^TypeError: First argument must be a string or Buffer$/);

assert.throws(() => {
  const outgoingMessage = new OutgoingMessage();
  outgoingMessage.write.call({_header: 'test', _hasBody: 'test'}, 1);
}, /^TypeError: First argument must be a string or Buffer$/);

// addTrailers
assert.throws(() => {
  const outgoingMessage = new OutgoingMessage();
  outgoingMessage.addTrailers();
}, /^TypeError: Cannot convert undefined or null to object$/);

assert.throws(() => {
  const outgoingMessage = new OutgoingMessage();
  outgoingMessage.addTrailers({'あ': 'value'});
}, /^TypeError: Trailer name must be a valid HTTP Token \["あ"\]$/);

assert.throws(() => {
  const outgoingMessage = new OutgoingMessage();
  outgoingMessage.addTrailers({404: 'あ'});
}, /^TypeError: The trailer content contains invalid characters$/);
