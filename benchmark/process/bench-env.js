'use strict';

const common = require('../common');

const bench = common.createBenchmark(main, {
  n: [1e6],
  operation: ['get', 'set', 'enumerate', 'query', 'delete'],
});


function main({ n, operation }) {
  switch (operation) {
    case 'get':
      bench.start();
      for (let i = 0; i < n; i++) {
        process.env.PATH; // eslint-disable-line no-unused-expressions
      }
      bench.end(n);
      break;
    case 'set':
      bench.start();
      for (let i = 0; i < n; i++) {
        process.env.DUMMY = 'hello, world';
      }
      bench.end(n);
      break;
    case 'enumerate':
      // First, normalize process.env so that benchmark results are comparable.
      for (const key of Object.keys(process.env))
        delete process.env[key];
      for (let i = 0; i < 64; i++)
        process.env[Math.random()] = Math.random();

      n /= 10;  // Enumeration is comparatively heavy.
      bench.start();
      for (let i = 0; i < n; i++) {
        // Access every item in object to process values.
        Object.keys(process.env);
      }
      bench.end(n);
      break;
    case 'query':
      bench.start();
      for (let i = 0; i < n; i++) {
        'PATH' in process.env; // eslint-disable-line no-unused-expressions
      }
      bench.end(n);
      break;
    case 'delete':
      bench.start();
      for (let i = 0; i < n; i++) {
        delete process.env.DUMMY;
      }
      bench.end(n);
      break;
  }
}
