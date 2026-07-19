'use strict';
const common = require('../common');

const assert = require('assert');
const { Buffer } = require('buffer');
const { Duplex, Writable } = require('stream');
const { suite, test } = require('node:test');

const ctors = [ArrayBuffer, SharedArrayBuffer];

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

suite('underlying Duplex', () => {
  suite('in non-object mode', () => {
    for (const ctor of ctors) {
      test(`converts ${ctor.name} chunks`, async () => {
        const buffer = new ctor(4);
        const duplex = new Duplex({
          writableObjectMode: false,
          write: common.mustCall((chunk, encoding, callback) => {
            assert(Buffer.isBuffer(chunk));
            assert.strictEqual(chunk.buffer, buffer);
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
