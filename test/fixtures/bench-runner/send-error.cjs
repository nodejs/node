'use strict';

if (process.env.NODE_BENCH_SEND_ERROR === 'callback') {
  process.send = (_message, _handle, _options, callback) => {
    callback(new Error('benchmark send callback failed'));
  };
} else {
  process.send = () => { throw new Error('benchmark send threw'); };
}

const { bench } = require('node:bench');

bench('send error', { samples: 1 }, (b) => {
  b.record({ duration_ns: 1n, operations: 1 });
});
