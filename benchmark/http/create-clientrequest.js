'use strict';

const common = require('../common.js');
const { ClientRequest } = require('http');
const assert = require('assert');

const types = Object.keys(common.urls)
  .filter((i) => common.urls[i]
  .startsWith('http://'));
const bench = common.createBenchmark(main, {
  // Use 'url' to avoid name clash with other http benchmark
  url: types.concat(['wpt']),
  arg: ['URL', 'string', 'options'],
  e: [1],
});

function noop() {}

function main({ url: type, arg, e }) {
  e = Number(e);
  const data = common.bakeUrlData(type, e, false, false)
    .filter((i) => i.startsWith('http://'));
  const len = data.length;
  let result;
  switch (arg) {
    case 'options': {
      const options = data.map((i) => ({
        path: new URL(i).path, createConnection: noop,
      }));
      bench.start();
      for (let i = 0; i < len; i++) {
        result = new ClientRequest(options[i]);
      }
      bench.end(len);
      break;
    }
    case 'URL': {
      const options = data.map((i) => new URL(i));
      bench.start();
      for (let i = 0; i < len; i++) {
        result = new ClientRequest(options[i], { createConnection: noop });
      }
      bench.end(len);
      break;
    }
    case 'string': {
      bench.start();
      for (let i = 0; i < len; i++) {
        result = new ClientRequest(data[i], { createConnection: noop });
      }
      bench.end(len);
      break;
    }
    default: {
      throw new Error(`Unknown arg type ${arg}`);
    }
  }
  assert.ok(result);
}
