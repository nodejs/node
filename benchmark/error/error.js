'use strict';

const {
  ERR_INVALID_THIS,
} = require('internal/errors').codes;
const common = require('../common.js');

const bench = common.createBenchmark(main, {
  type: ['Error', 'DOMException', 'MyExtendedError', 'SyntaxError', 'ERR_INVALID_THIS', 'MyCustomError'],
  n: [1e6],
}, {flags: ['--expose-internals']});


class MyExtendedError extends Error {
}



function main({ n, type }) {
  switch (type) {
    case 'Error': {
      bench.start();
      for (let i = 0; i < n; ++i)
        new Error('test');
      bench.end(n);
      break;
    }

    case 'DOMException': {
      bench.start();
      for (let i = 0; i < n; ++i)
        new DOMException('test');
      bench.end(n);
      break;
    }

    case 'MyExtendedError': {
      bench.start();
      for (let i = 0; i < n; ++i)
        new MyExtendedError('test');
      bench.end(n);
      break;
    }

    case 'SyntaxError': {
      bench.start();
      for (let i = 0; i < n; ++i)
        new SyntaxError('test');
      bench.end(n);
      break;
    }

    case 'ERR_INVALID_THIS': {
      bench.start();
      for (let i = 0; i < n; ++i)
        new ERR_INVALID_THIS('test');
      bench.end(n);
      break;
    }

    case 'MyCustomError': {
      bench.start();
      for (let i = 0; i < n; ++i)
        new MyCustomError('test');
      bench.end(n);
      break;
    }

    default:
      throw new Error('Unknown type');
  }
}
