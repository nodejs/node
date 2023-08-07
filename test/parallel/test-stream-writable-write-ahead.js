'use strict';
const common = require('../common');
const assert = require('assert');

const { Writable } = require('stream');

{
  const w = new Writable({
    writev: common.mustCall((chunks, cb) => {
      assert.deepStrictEqual(
        Buffer('4213'),
        Buffer.concat(chunks.map(({ chunk }) => chunk)),
      );
      cb();
    })
  });
  w.cork();
  w.write('1');
  w.writeAhead('2');
  w.write('3');
  w.writeAhead('4');
  w.end();
}

{
  const items = [
    { a: 1 },
    { b: 2 },
    { c: 3 },
    { d: 4 },
  ];
  const w = new Writable({
    objectMode: true,
    writev: common.mustCall((chunks, cb) => {
      assert.deepStrictEqual(
        [
          items[2],
          items[0],
          items[1],
          items[3],
        ],
        chunks.map(({ chunk }) => chunk),
      );
      cb();
    }),
  });
  w.cork();
  w.writeAhead(items[0]);
  w.write(items[1]);
  w.writeAhead(items[2]);
  w.write(items[3]);
  w.end();
}
