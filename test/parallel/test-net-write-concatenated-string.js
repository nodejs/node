'use strict';
const common = require('../common');
const assert = require('assert');
const net = require('net');

// Writes concatenated strings around the size limits of the stack buffer used
// by StreamBase::WriteString() and checks the bytes on the wire.

function concat(length, base) {
  let str = '';
  for (let i = 0; i < length; i++)
    str += String.fromCharCode(base + i % 16);
  return str;
}

const cases = [];
for (const length of [0, 13, 5461, 5462, 16384, 16385]) {
  // ASCII, non-ASCII Latin-1, two-byte.
  for (const base of [0x61, 0xe0, 0x2000]) {
    for (const encoding of ['utf8', 'latin1', 'ascii']) {
      cases.push({ length, base, encoding });
    }
  }
}

const expected = [];

const server = net.createServer(common.mustCall((socket) => {
  const chunks = [];
  socket.on('data', (chunk) => chunks.push(chunk));
  socket.on('end', common.mustCall(() => {
    const actual = Buffer.concat(chunks);
    let offset = 0;
    for (const [i, buf] of expected.entries()) {
      const { length, base, encoding } = cases[i];
      assert.deepStrictEqual(
        actual.subarray(offset, offset + buf.length),
        buf,
        `length ${length}, base 0x${base.toString(16)}, ${encoding}`);
      offset += buf.length;
    }
    assert.strictEqual(offset, actual.length);
    server.close();
  }));
}));

server.listen(0, common.mustCall(() => {
  const socket = net.connect(server.address().port);
  let i = 0;
  function next() {
    if (i === cases.length) return socket.end();
    const { length, base, encoding } = cases[i++];
    const str = concat(length, base);
    // Each write waits for the previous one so that it goes through
    // WriteString() and not Writev().
    socket.write(str, encoding, common.mustSucceed(next));
    expected.push(Buffer.from(str, encoding));
  }
  socket.on('connect', common.mustCall(next));
}));
