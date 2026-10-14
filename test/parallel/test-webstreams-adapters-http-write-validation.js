'use strict';

const common = require('../common');
const assert = require('assert');
const { IncomingMessage, OutgoingMessage, ServerResponse } = require('http');
const { Writable } = require('stream');

function createResponse() {
  const socket = new Writable({
    write(chunk, encoding, callback) { callback(); },
  });
  const request = new IncomingMessage(socket);
  request.method = 'GET';
  request.httpVersionMajor = 1;
  request.httpVersionMinor = 1;
  const response = new ServerResponse(request);
  response.assignSocket(socket);
  response.on('error', common.mustNotCall());
  return response;
}

async function checkNativeValidation(resizeAfterEnqueue) {
  const backing = new ArrayBuffer(32, { maxByteLength: 64 });
  const input = new DataView(backing, 16, 8);
  if (!resizeAfterEnqueue) {
    backing.resize(8);
    // Let the queuing strategy admit the view so the native type validation
    // is exercised even though the view's intrinsic metadata is out of bounds.
    Object.defineProperty(input, 'byteLength', { value: 8 });
  }
  const writer = Writable.toWeb(createResponse()).getWriter();
  const pending = writer.write(input);
  if (resizeAfterEnqueue) backing.resize(8);
  await Promise.all([
    assert.rejects(pending, { code: 'ERR_INVALID_ARG_TYPE' }),
    assert.rejects(writer.closed, { code: 'ERR_INVALID_ARG_TYPE' }),
  ]);
}

async function checkOverriddenWrite() {
  const response = createResponse();
  const writer = Writable.toWeb(response).getWriter();
  let retained;
  response.write = common.mustCall((chunk) => {
    retained = chunk;
    return true;
  });
  const input = new DataView(new ArrayBuffer(4));
  input.setUint32(0, 0x01020304);
  await writer.write(input);
  input.setUint32(0, 0x09090909);
  assert(retained instanceof DataView);
  assert.notStrictEqual(retained.buffer, input.buffer);
  assert.strictEqual(retained.getUint32(0), 0x01020304);
  // A real HTTP server closes the response after finish. Let the test socket
  // deliver that close event so the adapter can observe the completed stream.
  response.once('finish', common.mustCall(() => response.socket.destroy()));
  await writer.close();
}

async function checkPatchedWrite() {
  const originalWrite = OutgoingMessage.prototype.write;
  OutgoingMessage.prototype.write = common.mustNotCall();
  try {
    const backing = new ArrayBuffer(32, { maxByteLength: 64 });
    const input = new DataView(backing, 16, 8);
    backing.resize(8);
    Object.defineProperty(input, 'byteLength', { value: 8 });
    const writer = Writable.toWeb(createResponse()).getWriter();
    const expected = common.mustCall((error) => error instanceof TypeError && error.code === undefined, 2);
    await Promise.all([
      assert.rejects(writer.write(input), expected),
      assert.rejects(writer.closed, expected),
    ]);
  } finally {
    OutgoingMessage.prototype.write = originalWrite;
  }
}

async function main() {
  for (const resizeAfterEnqueue of [false, true]) await checkNativeValidation(resizeAfterEnqueue);
  await checkOverriddenWrite();
  await checkPatchedWrite();
}

main().then(common.mustCall());
