'use strict';

const common = require('../common');
const Readable = require('stream').Readable;

const bench = common.createBenchmark(main, {
  n: [1e7],
  type: ['array', 'sync-generator-with-sync-values', 'sync-generator-with-async-values', 'async-generator'],
});

async function main({ n, type }) {
  let fromArg;

  switch (type) {
    case 'array': {
      fromArg = [];
      for (let i = 0; i < n; i++) {
        fromArg.push(`${i}`);
      }

      break;
    }

    case 'sync-generator-with-sync-values': {
      fromArg = (function* () {
        for (let i = 0; i < n; i++) {
          yield `${i}`;
        }
      })();

      break;
    }

    case 'sync-generator-with-async-values': {
      fromArg = (function* () {
        for (let i = 0; i < n; i++) {
          yield Promise.resolve(`${i}`);
        }
      })();

      break;
    }

    case 'async-generator': {
      fromArg = (async function* () {
        for (let i = 0; i < n; i++) {
          yield `${i}`;
        }
      })();

      break;
    }

    default: {
      throw new Error(`Unknown type: ${type}`);
    }
  }

  const s = new Readable.from(fromArg);

  bench.start();
  s.on('data', (data) => {
    // eslint-disable-next-line no-unused-expressions
    data;
  });
  s.on('close', () => {
    bench.end(n);
  });
}
