'use strict';
const common = require('../common.js');

const bench = common.createBenchmark(main, {
  withBase: ['true', 'false'],
  type: ['wpt'],  // Too many combinations - just use WPT by default
  e: [1],
  prop: ['href', 'origin', 'protocol',
         'username', 'password', 'host', 'hostname', 'port',
         'pathname', 'search', 'searchParams', 'hash']
});

function setAndGet(data, prop) {
  const len = data.length;
  var result = data[0][prop];
  bench.start();
  for (var i = 0; i < len; ++i) {
    result = data[i][prop];
    data[i][prop] = result;
  }
  bench.end(len);
  return result;
}

function get(data, prop) {
  const len = data.length;
  var result = data[0][prop];
  bench.start();
  for (var i = 0; i < len; ++i) {
    result = data[i][prop]; // get
  }
  bench.end(len);
  return result;
}

function main({ e, type, prop, withBase }) {
  e = +e;
  withBase = withBase === 'true';
  const data = common.bakeUrlData(type, e, withBase, true);
  switch (prop) {
    case 'protocol':
    case 'username':
    case 'password':
    case 'host':
    case 'hostname':
    case 'port':
    case 'pathname':
    case 'search':
    case 'hash':
    case 'href':
      setAndGet(data, prop);
      break;
    case 'origin':
    case 'searchParams':
      get(data, prop);
      break;
    default:
      throw new Error('Unknown prop');
  }
}
