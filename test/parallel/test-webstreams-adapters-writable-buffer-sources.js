'use strict';
const common = require('../common');

const assert = require('assert');
const { Buffer } = require('buffer');
const { ServerResponse } = require('http');
const { Duplex, Writable } = require('stream');
const { suite, test } = require('node:test');

const ctors = [ArrayBuffer, SharedArrayBuffer];

function createServerResponse() {
  const socket = new Writable({
    write(chunk, encoding, callback) {
      callback();
    },
  });
  const response = new ServerResponse({
    method: 'GET',
    httpVersionMajor: 1,
    httpVersionMinor: 1,
  });
  socket.on('error', common.mustNotCall());
  response.on('error', common.mustNotCall());
  response.assignSocket(socket);
  return response;
}

async function completesWithin(promise) {
  let timer;
  try {
    return await Promise.race([
      promise,
      new Promise((_, reject) => {
        timer = setTimeout(
          () => reject(new Error('write timed out')),
          common.platformTimeout(1000),
        );
      }),
    ]);
  } finally {
    clearTimeout(timer);
  }
}

suite('underlying Writable', () => {
  suite('in non-object mode', () => {
    for (const ctor of ctors) {
      test(`converts ${ctor.name} chunks`, async () => {
        const buffer = new ctor(4);
        const writable = new Writable({
          objectMode: false,
          write: common.mustCall((chunk, encoding, callback) => {
            assert(Buffer.isBuffer(chunk));
            assert.strictEqual(chunk.buffer, buffer);
            callback();
          }),
        });
        writable.on('error', common.mustNotCall());
        const writer = Writable.toWeb(writable).getWriter();
        await writer.write(buffer);
      });
    }

    for (const highWaterMark of [1, 16]) {
      test(`waits for mutable chunks with highWaterMark ${highWaterMark}`,
           async () => {
             let finishWrite;
             let consumed;
             const writable = new Writable({
               highWaterMark,
               write(chunk, encoding, callback) {
                 finishWrite = () => {
                   consumed = Buffer.from(chunk);
                   callback();
                 };
               },
             });
             writable.on('error', common.mustNotCall());
             const writer = Writable.toWeb(writable).getWriter();
             const input = new Uint8Array([1, 2, 3, 4]);
             let settled = false;
             const writePromise = writer.write(input).then(() => {
               settled = true;
             });

             await new Promise(setImmediate);
             assert.strictEqual(settled, false);
             finishWrite();
             await writePromise;
             input.fill(9);
             await writer.close();

             assert.deepStrictEqual(
               consumed,
               Buffer.from([1, 2, 3, 4]),
             );
           });
    }

    test('copies mutable chunks while the Writable is corked', async () => {
      let consumed;
      const writable = new Writable({
        write(chunk, encoding, callback) {
          consumed = Buffer.from(chunk);
          callback();
        },
      });
      writable.on('error', common.mustNotCall());
      writable.cork();
      const writer = Writable.toWeb(writable).getWriter();
      const input = new Uint8Array([1, 2, 3, 4]);

      await writer.write(input);
      input.fill(9);
      writable.uncork();
      await writer.close();

      assert.deepStrictEqual(consumed, Buffer.from([1, 2, 3, 4]));
    });

    test('copies mutable chunks when write() is overridden', async () => {
      let consumed;
      let received;
      let notifyConsumed;
      const consumedPromise = new Promise((resolve) => {
        notifyConsumed = resolve;
      });
      const writable = new Writable({
        write(chunk, encoding, callback) {
          callback();
        },
      });
      writable.on('error', common.mustNotCall());
      const writer = Writable.toWeb(writable).getWriter();
      writable.write = common.mustCall((chunk) => {
        received = chunk;
        setImmediate(() => {
          consumed = Buffer.from(chunk);
          notifyConsumed();
        });
        return true;
      });
      const input = new Uint8Array([1, 2, 3, 4]);

      await writer.write(input);
      input.fill(9);
      await consumedPromise;
      await writer.close();

      assert.notStrictEqual(received.buffer, input.buffer);
      assert.deepStrictEqual(consumed, Buffer.from([1, 2, 3, 4]));
    });

    test('does not trust a patched Writable.prototype.write', async () => {
      const originalWrite = Writable.prototype.write;
      let consumed;
      let notifyConsumed;
      const consumedPromise = new Promise((resolve) => {
        notifyConsumed = resolve;
      });
      const writable = new Writable({
        write(chunk, encoding, callback) {
          setImmediate(() => {
            consumed = Buffer.from(chunk);
            callback();
            notifyConsumed();
          });
        },
      });
      writable.on('error', common.mustNotCall());
      let writer;

      Writable.prototype.write = function(chunk) {
        return originalWrite.call(this, chunk);
      };
      try {
        writer = Writable.toWeb(writable).getWriter();
        const input = new Uint8Array([1, 2, 3, 4]);
        await completesWithin(writer.write(input));
        input.fill(9);
        await consumedPromise;

        assert.deepStrictEqual(consumed, Buffer.from([1, 2, 3, 4]));
      } finally {
        Writable.prototype.write = originalWrite;
      }
      await writer.close();
    });

    test('reads write() once for classification and invocation', async () => {
      const originalWrite = Writable.prototype.write;
      let consumed;
      const writable = new Writable({
        write(chunk, encoding, callback) {
          setImmediate(() => {
            consumed = Buffer.from(chunk);
            callback();
          });
        },
      });
      writable.on('error', common.mustNotCall());
      const writer = Writable.toWeb(writable).getWriter();
      let writeAccesses = 0;
      Object.defineProperty(writable, 'write', {
        configurable: true,
        get() {
          writeAccesses++;
          return writeAccesses === 1 ?
            originalWrite :
            function callbacklessWrite(chunk) {
              return originalWrite.call(this, chunk);
            };
        },
      });

      const input = new Uint8Array([1, 2, 3, 4]);
      await completesWithin(writer.write(input));
      input.fill(9);

      assert.strictEqual(writeAccesses, 1);
      assert.deepStrictEqual(consumed, Buffer.from([1, 2, 3, 4]));
      delete writable.write;
      await writer.close();
    });

    test('preserves cloned view brands and SharedArrayBuffer backing',
         async () => {
           const dataView = new DataView(
             Uint8Array.from([0, 1, 2, 3, 4, 0]).buffer,
             1,
             4,
           );
           const uint16Buffer = new ArrayBuffer(6);
           const uint16 = new Uint16Array(uint16Buffer, 2, 2);
           new Uint8Array(uint16Buffer, 2, 4).set([1, 2, 3, 4]);
           const shared = new SharedArrayBuffer(6);
           const sharedView = new Uint8Array(shared, 1, 4);
           sharedView.set([1, 2, 3, 4]);
           const inputs = [
             Buffer.from([1, 2, 3, 4]),
             dataView,
             uint16,
             sharedView,
           ];
           const expected = inputs.map((chunk) => ({
             brand: Buffer.isBuffer(chunk) ?
               'Buffer' : Object.prototype.toString.call(chunk),
             bytes: Buffer.from(new Uint8Array(
               chunk.buffer,
               chunk.byteOffset,
               chunk.byteLength,
             )),
             shared: chunk.buffer instanceof SharedArrayBuffer,
           }));
           const received = [];
           const writable = new Writable({
             write(chunk, encoding, callback) {
               callback();
             },
           });
           writable.on('error', common.mustNotCall());
           const writer = Writable.toWeb(writable).getWriter();
           writable.write = common.mustCall((chunk) => {
             received.push(chunk);
             return true;
           }, inputs.length);

           for (const chunk of inputs) {
             await writer.write(chunk);
             new Uint8Array(
               chunk.buffer,
               chunk.byteOffset,
               chunk.byteLength,
             ).fill(9);
           }
           await writer.close();

           for (let i = 0; i < received.length; i++) {
             const actual = received[i];
             const actualBrand = Buffer.isBuffer(actual) ?
               'Buffer' : Object.prototype.toString.call(actual);
             assert.strictEqual(actualBrand, expected[i].brand);
             assert.notStrictEqual(actual.buffer, inputs[i].buffer);
             assert.strictEqual(
               actual.buffer instanceof SharedArrayBuffer,
               expected[i].shared,
             );
             assert.deepStrictEqual(
               Buffer.from(new Uint8Array(
                 actual.buffer,
                 actual.byteOffset,
                 actual.byteLength,
               )),
               expected[i].bytes,
             );
           }
         });
  });

  suite('in object mode', () => {
    for (const ctor of ctors) {
      test(`passes through ${ctor.name} chunks`, async () => {
        const buffer = new ctor(4);
        const writable = new Writable({
          objectMode: true,
          write: common.mustCall((chunk, encoding, callback) => {
            assert(chunk instanceof ctor);
            assert.strictEqual(chunk, buffer);
            callback();
          }),
        });
        writable.on('error', common.mustNotCall());
        const writer = Writable.toWeb(writable).getWriter();
        await writer.write(buffer);
      });
    }
  });
});

suite('underlying ServerResponse', () => {
  test('rejects invalid view types before cloning', async () => {
    const response = createServerResponse();
    const writer = Writable.toWeb(response).getWriter();

    try {
      await Promise.all([
        assert.rejects(writer.write(new DataView(new ArrayBuffer(4))), {
          code: 'ERR_INVALID_ARG_TYPE',
        }),
        assert.rejects(writer.closed, {
          code: 'ERR_INVALID_ARG_TYPE',
        }),
      ]);
    } finally {
      await new Promise((resolve) => response.end(resolve));
    }
  });

  test('preserves write() overrides', async () => {
    const response = createServerResponse();
    const writer = Writable.toWeb(response).getWriter();
    const originalWrite = response.write;
    let received;
    response.write = common.mustCall((chunk) => {
      received = chunk;
      return true;
    });
    const input = new DataView(Uint8Array.from([1, 2, 3, 4]).buffer);

    try {
      await writer.write(input);
      new Uint8Array(input.buffer).fill(9);
      assert(received instanceof DataView);
      assert.notStrictEqual(received.buffer, input.buffer);
      assert.deepStrictEqual(
        Buffer.from(received.buffer),
        Buffer.from([1, 2, 3, 4]),
      );
    } finally {
      response.write = originalWrite;
      await new Promise((resolve) => response.end(resolve));
    }
  });
});

suite('underlying Duplex', () => {
  suite('in non-object mode', () => {
    for (const ctor of ctors) {
      test(`copies ${ctor.name} chunks`, async () => {
        const buffer = new ctor(4);
        new Uint8Array(buffer).set([1, 2, 3, 4]);
        const duplex = new Duplex({
          writableObjectMode: false,
          write: common.mustCall((chunk, encoding, callback) => {
            assert(Buffer.isBuffer(chunk));
            assert.notStrictEqual(chunk.buffer, buffer);
            assert.strictEqual(
              chunk.buffer instanceof SharedArrayBuffer,
              buffer instanceof SharedArrayBuffer,
            );
            assert.deepStrictEqual(
              chunk,
              Buffer.from([1, 2, 3, 4]),
            );
            callback();
          }),
          read() {
            this.push(null);
          },
        });
        duplex.on('error', common.mustNotCall());
        const writer = Duplex.toWeb(duplex).writable.getWriter();
        await writer.write(buffer);
      });
    }

    test('copies mutable chunks without waiting for the write callback',
         async () => {
           let consumed;
           let finishWrite;
           const duplex = new Duplex({
             write(chunk, encoding, callback) {
               finishWrite = () => {
                 consumed = Buffer.from(chunk);
                 callback();
               };
             },
             read() {
               this.push(null);
             },
           });
           duplex.on('error', common.mustNotCall());
           const writer = Duplex.toWeb(duplex).writable.getWriter();
           const input = new Uint8Array([1, 2, 3, 4]);

           await writer.write(input);
           input.fill(9);
           finishWrite();
           await new Promise(setImmediate);

           assert.deepStrictEqual(
             consumed,
             Buffer.from([1, 2, 3, 4]),
           );
           await writer.close();
         });
  });

  suite('in object mode', () => {
    for (const ctor of ctors) {
      test(`passes through ${ctor.name} chunks`, async () => {
        const buffer = new ctor(4);
        const duplex = new Duplex({
          writableObjectMode: true,
          write: common.mustCall((chunk, encoding, callback) => {
            assert(chunk instanceof ctor);
            assert.strictEqual(chunk, buffer);
            callback();
          }),
          read() {
            this.push(null);
          },
        });
        duplex.on('error', common.mustNotCall());
        const writer = Duplex.toWeb(duplex).writable.getWriter();
        await writer.write(buffer);
      });
    }
  });
});
