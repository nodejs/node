'use strict';

const common = require('../common');

const assert = require('node:assert');
const { Duplex, Readable } = require('node:stream');

function create(construct, objectMode = false) {
  const chunks = objectMode ? [{ a: 1 }, { b: 2 }] : ['a', 'b'];
  return new Readable({
    highWaterMark: 0,
    objectMode,
    construct,
    read() {
      this.push(chunks.length ? chunks.shift() : null);
    },
  });
}

create((cb) => setImmediate(cb)).toArray().then(common.mustCall((data) => {
  assert.deepStrictEqual(data.map(String), ['a', 'b']);
}));

create((cb) => cb()).toArray().then(common.mustCall((data) => {
  assert.deepStrictEqual(data.map(String), ['a', 'b']);
}));

create((cb) => setImmediate(cb), true).toArray().then(common.mustCall((data) => {
  assert.deepStrictEqual(data, [{ a: 1 }, { b: 2 }]);
}));

(async () => {
  const data = [];
  for await (const chunk of create((cb) => setImmediate(cb))) {
    data.push(String(chunk));
  }
  assert.deepStrictEqual(data, ['a', 'b']);
})().then(common.mustCall());

{
  const stream = create((cb) => setImmediate(cb));
  const data = [];
  stream.on('readable', () => {
    let chunk;
    while ((chunk = stream.read()) !== null) data.push(String(chunk));
  });
  stream.on('end', common.mustCall(() => {
    assert.deepStrictEqual(data, ['a', 'b']);
  }));
}

{
  const stream = create((cb) => setImmediate(cb));
  const data = [];
  stream.on('data', (chunk) => data.push(String(chunk)));
  stream.on('end', common.mustCall(() => {
    assert.deepStrictEqual(data, ['a', 'b']);
  }));
}

{
  const chunks = ['a', 'b'];
  const duplex = new Duplex({
    readableHighWaterMark: 0,
    construct(cb) {
      setImmediate(cb);
    },
    read() {
      this.push(chunks.length ? chunks.shift() : null);
    },
    write: common.mustNotCall(),
  });
  duplex.toArray().then(common.mustCall((data) => {
    assert.deepStrictEqual(data.map(String), ['a', 'b']);
  }));
}

new Readable({
  highWaterMark: 0,
  construct: common.mustCall((cb) => setImmediate(cb)),
  read: common.mustNotCall(),
});
