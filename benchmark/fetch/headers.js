'use strict';
const common = require('../common.js');

const bench = common.createBenchmark(main, {
  n: [1e5],
  method: [
    'construct-empty',
    'construct-object',
    'construct-headers',
    'get',
    'get-common',
    'set',
    'append',
    'has',
    'delete',
    'iterate',
  ],
});

const objectInit = {
  'Accept': 'application/json',
  'Content-Type': 'text/plain',
  'User-Agent': 'benchmark',
  'Authorization': 'Bearer token',
  'Cookie': 'a=1',
  'X-Request-Id': 'abc',
  'Cache-Control': 'no-cache',
  'Host': 'example.com',
};

function main({ n, method }) {
  const headers = new Headers(objectInit);
  const copySource = new Headers(objectInit);
  let result;

  bench.start();
  switch (method) {
    case 'construct-empty':
      for (let i = 0; i < n; i++)
        new Headers();
      break;
    case 'construct-object':
      for (let i = 0; i < n; i++)
        new Headers(objectInit);
      break;
    case 'construct-headers':
      for (let i = 0; i < n; i++)
        new Headers(copySource);
      break;
    case 'get':
      for (let i = 0; i < n; i++)
        result = headers.get('x-request-id');
      break;
    case 'get-common':
      for (let i = 0; i < n; i++)
        result = headers.get('content-type');
      break;
    case 'set':
      for (let i = 0; i < n; i++)
        headers.set('x-count', i);
      break;
    case 'append':
      for (let i = 0; i < n; i++) {
        const current = new Headers();
        current.append('Accept', 'text/html');
        current.append('X-Custom', i);
      }
      break;
    case 'has':
      for (let i = 0; i < n; i++)
        result = headers.has('authorization');
      break;
    case 'delete': {
      for (let i = 0; i < n; i++) {
        const current = new Headers(objectInit);
        current.delete('content-type');
      }
      break;
    }
    case 'iterate':
      for (let i = 0; i < n; i++) {
        for (const entry of headers)
          result = entry;
      }
      break;
    default:
      throw new Error(`Unexpected method "${method}"`);
  }
  bench.end(n);

  // Keep a live use so V8 cannot DCE the loop.
  if (result === Symbol.for('benchmark-never'))
    throw new Error('unreachable');
}
