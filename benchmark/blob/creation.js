'use strict';
const common = require('../common.js');
const {
  Blob,
} = require('node:buffer');
const assert = require('assert');

const options = {
  flags: ['--expose-internals'],
};

const bench = common.createBenchmark(main, {
  n: [50e3],
  kind: [
    'Blob',
    'createBlob',
  ],
}, options);

let _blob;
let _createBlob;

function main({ n, kind }) {
  switch (kind) {
    case 'Blob': {
      bench.start();
      for (let i = 0; i < n; ++i)
        _blob = new Blob(['hello']);
      bench.end(n);

      // Avoid V8 deadcode (elimination)
      assert.ok(_blob);
      break;
    } case 'createBlob': {
      const { createBlob } = require('internal/blob');
      const reusableBlob = new Blob(['hello']);
      bench.start();
      for (let i = 0; i < n; ++i)
        _createBlob = createBlob(reusableBlob, reusableBlob.size);
      bench.end(n);

      // Avoid V8 deadcode (elimination)
      assert.ok(_createBlob);
      break;
    } default:
      throw new Error('Invalid kind');
  }
}
