'use strict';
const common = require('../common');
const assert = require('assert');

const { Readable, Writable } = require('stream');

const ABC = new Uint8Array([0x41, 0x42, 0x43]);
const DEF = new Uint8Array([0x44, 0x45, 0x46]);
const GHI = new Uint8Array([0x47, 0x48, 0x49]);

{
  // Simple Writable test.

  let n = 0;
  const writable = new Writable({
    write: common.mustCall((chunk, encoding, cb) => {
      assert(chunk instanceof Buffer);
      if (n++ === 0) {
        assert.strictEqual(String(chunk), 'ABC');
      } else {
        assert.strictEqual(String(chunk), 'DEF');
      }

      cb();
    }, 2)
  });

  writable.write(ABC);
  writable.end(DEF);
}

{
  // Writable test, pass in Uint8Array in object mode.

  const writable = new Writable({
    objectMode: true,
    write: common.mustCall((chunk, encoding, cb) => {
      assert(!(chunk instanceof Buffer));
      assert(chunk instanceof Uint8Array);
      assert.strictEqual(chunk, ABC);
      assert.strictEqual(encoding, undefined);
      cb();
    })
  });

  writable.end(ABC);
}

{
  // Writable test, multiple writes carried out via writev.
  let callback;

  const writable = new Writable({
    write: common.mustCall((chunk, encoding, cb) => {
      assert(chunk instanceof Buffer);
      assert.strictEqual(encoding, 'buffer');
      assert.strictEqual(String(chunk), 'ABC');
      callback = cb;
    }),
    writev: common.mustCall((chunks, cb) => {
      assert.strictEqual(chunks.length, 2);
      assert.strictEqual(chunks[0].encoding, 'buffer');
      assert.strictEqual(chunks[1].encoding, 'buffer');
      assert.strictEqual(chunks[0].chunk + chunks[1].chunk, 'DEFGHI');
    })
  });

  writable.write(ABC);
  writable.write(DEF);
  writable.end(GHI);
  callback();
}

{
  // Simple Readable test.
  const readable = new Readable({
    read() {}
  });

  readable.push(DEF);
  readable.unshift(ABC);

  const buf = readable.read();
  assert(buf instanceof Buffer);
  assert.deepStrictEqual([...buf], [...ABC, ...DEF]);
}

{
  // Readable test, setEncoding.
  const readable = new Readable({
    read() {}
  });

  readable.setEncoding('utf8');

  readable.push(DEF);
  readable.unshift(ABC);

  const out = readable.read();
  assert.strictEqual(out, 'ABCDEF');
}
