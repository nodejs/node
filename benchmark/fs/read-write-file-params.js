'use strict';

const common = require('../common.js');
const fs = require('fs');
const path = require('path');

const filePath = path.join(__dirname, 'read-write-file-params.txt');
const data = Buffer.from('1');
const options = {};
const cb = () => {};

const configs = {
  n: [1e3],
  methodName: ['readFile', 'writeFile'],
  withOptions: ['true', 'false'],
};

const bench = common.createBenchmark(main, configs);

function main(conf) {
  const n = +conf.n;
  const methodName = conf.methodName;
  const withOptions = conf.withOptions;

  const method = fs[methodName];

  switch (methodName) {
    case 'readFile':
      switch (withOptions) {
        case 'true':
          bench.start();
          for (let i = 0; i < n; i++) method(filePath, options, cb);
          bench.end(n);
          break;
        case 'false':
          bench.start();
          for (let i = 0; i < n; i++) method(filePath, cb);
          bench.end(n);
          break;
      }
      break;
    case 'writeFile':
      switch (withOptions) {
        case 'true':
          bench.start();
          for (let i = 0; i < n; i++) method(filePath, data, options, cb);
          bench.end(n);
          break;
        case 'false':
          bench.start();
          for (let i = 0; i < n; i++) method(filePath, data, cb);
          bench.end(n);
          break;
      }
      break;
  }
}
