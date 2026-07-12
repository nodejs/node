'use strict';

// A console throughput benchmark.
// Uses a custom Console with null Writable streams to avoid I/O latency.

const common = require('../common.js');
const { Writable } = require('stream');
const { Console } = require('console');

const bench = common.createBenchmark(main, {
  n: [2e6],
  variant: ['plain', 'format', 'object', 'group', 'info', 'warn', 'error'],
});

class Null extends Writable {
  _write(chunk, enc, cb) { cb(); }
}

function makeConsole() {
  const dn = new Null();
  return new Console({ stdout: dn, stderr: dn, ignoreErrors: true, colorMode: false });
}

function main({ n, variant }) {
  const c = makeConsole();

  switch (variant) {
    case 'plain': {
      bench.start();
      for (let i = 0; i < n; i++) c.log('hello world');
      bench.end(n);
      break;
    }
    case 'format': {
      bench.start();
      for (let i = 0; i < n; i++) c.log('%s %d %j', 'a', 42, { x: 1 });
      bench.end(n);
      break;
    }
    case 'object': {
      const obj = { a: 1, b: 2, c: 3 };
      bench.start();
      for (let i = 0; i < n; i++) c.log(obj);
      bench.end(n);
      break;
    }
    case 'group': {
      bench.start();
      for (let i = 0; i < n; i++) {
        c.group('g');
        c.log('x');
        c.groupEnd();
      }
      bench.end(n);
      break;
    }
    case 'info': {
      bench.start();
      for (let i = 0; i < n; i++) c.info('hello world');
      bench.end(n);
      break;
    }
    case 'warn': {
      bench.start();
      for (let i = 0; i < n; i++) c.warn('hello world');
      bench.end(n);
      break;
    }
    case 'error': {
      bench.start();
      for (let i = 0; i < n; i++) c.error('hello world');
      bench.end(n);
      break;
    }
    default:
      throw new Error('unknown variant');
  }
}
