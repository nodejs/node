'use strict';
const common = require('../common');
const assert = require('assert');
const net = require('net');

// Writes concatenated strings with a Buffer after each of them in a single
// StreamBase::Writev() call, like an HTTP response header followed by a Buffer
// body, and checks the bytes on the wire.

function concat(length, base) {
  let str = '';
  for (let i = 0; i < length; i++)
    str += String.fromCharCode(base + i % 16);
  return str;
}

const separator = Buffer.from('|');
const writes = [];
// Writev() reserves 3 bytes per character for UTF-8 strings of up to 65535
// characters and the exact size for longer ones.
for (const length of [0, 13, 5461, 16385, 65535, 65536]) {
  // ASCII, non-ASCII Latin-1, two-byte.
  for (const base of [0x61, 0xe0, 0x2000]) {
    for (const encoding of ['utf8', 'latin1', 'ascii']) {
      writes.push({
        label: `length ${length}, base 0x${base.toString(16)}, ${encoding}`,
        str: concat(length, base),
        encoding,
      });
    }
  }
}

const server = net.createServer(common.mustCall((socket) => {
  const chunks = [];
  socket.on('data', (chunk) => chunks.push(chunk));
  socket.on('end', common.mustCall(() => {
    const actual = Buffer.concat(chunks);
    let offset = 0;
    for (const { label, str, encoding } of writes) {
      const expected = Buffer.concat([Buffer.from(str, encoding), separator]);
      assert.deepStrictEqual(
        actual.subarray(offset, offset + expected.length), expected, label);
      offset += expected.length;
    }
    assert.strictEqual(offset, actual.length);
    server.close();
  }));
}));

server.listen(0, common.mustCall(() => {
  const socket = net.connect(server.address().port, common.mustCall(() => {
    // Corked writes are flushed together with Writev().
    socket.cork();
    for (const { str, encoding } of writes) {
      socket.write(str, encoding);
      socket.write(separator);
    }
    socket.uncork();
    socket.end();
  }));
}));
