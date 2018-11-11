'use strict';

const common = require('../common.js');
const ClientRequest = require('http').ClientRequest;
const types = Object.keys(common.urls)
  .filter((i) => common.urls[i]
  .startsWith('http://'));
const bench = common.createBenchmark(main, {
  // Use 'url' to avoid name clash with other http benchmark
  url: types.concat(['wpt']),
  arg: ['URL', 'string', 'options'],
  e: [1]
});

function noop() {}

function main({ url: type, arg, e }) {
  e = +e;
  const data = common.bakeUrlData(type, e, false, false)
    .filter((i) => i.startsWith('http://'));
  const len = data.length;
  var result;
  var i;
  if (arg === 'options') {
    const options = data.map((i) => ({
      path: new URL(i).path, createConnection: noop
    }));
    bench.start();
    for (i = 0; i < len; i++) {
      result = new ClientRequest(options[i]);
    }
    bench.end(len);
  } else if (arg === 'URL') {
    const options = data.map((i) => new URL(i));
    bench.start();
    for (i = 0; i < len; i++) {
      result = new ClientRequest(options[i], { createConnection: noop });
    }
    bench.end(len);
  } else if (arg === 'string') {
    bench.start();
    for (i = 0; i < len; i++) {
      result = new ClientRequest(data[i], { createConnection: noop });
    }
    bench.end(len);
  } else {
    throw new Error(`Unknown arg type ${arg}`);
  }
  require('assert').ok(result);
}
