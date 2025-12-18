'use strict';

const common = require('../common');
const assert = require('assert');
const { MIMEType } = require('util');

const bench = common.createBenchmark(main, {
  n: [1e5],
  value: [
    'application/ecmascript; ',
    'text/html;charset=gbk',
    `text/html;${'0123456789'.repeat(12)}=x;charset=gbk`,
    'text/html;test=\u00FF;charset=gbk',
    'x/x;\n\r\t x=x\n\r\t ;x=y',
  ],
}, {
});

function main({ n, value }) {
  // Warm up.
  const length = 1024;
  const array = [];
  let errCase = false;

  const mime = new MIMEType(value);

  for (let i = 0; i < length; ++i) {
    try {
      array.push(mime.toString());
    } catch (e) {
      errCase = true;
      array.push(e);
    }
  }

  // console.log(`errCase: ${errCase}`);
  bench.start();

  for (let i = 0; i < n; ++i) {
    const index = i % length;
    try {
      array[index] = mime.toString();
    } catch (e) {
      array[index] = e;
    }
  }

  bench.end(n);

  // Verify the entries to prevent dead code elimination from making
  // the benchmark invalid.
  for (let i = 0; i < length; ++i) {
    assert.strictEqual(typeof array[i], errCase ? 'object' : 'string');
  }
}
