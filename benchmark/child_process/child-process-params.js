'use strict';

const common = require('../common.js');
const cp = require('child_process');

const command = 'echo';
const args = ['hello'];
const options = {};
const cb = () => {};

const configs = {
  n: [1e3],
  methodName: [
    'exec', 'execSync',
    'execFile', 'execFileSync',
    'spawn', 'spawnSync',
  ],
  params: [1, 2, 3, 4],
};

const bench = common.createBenchmark(main, configs);

function main({ n, methodName, params }) {
  const method = cp[methodName];

  switch (methodName) {
    case 'exec':
      switch (params) {
        case 1:
          bench.start();
          for (let i = 0; i < n; i++) method(command).kill();
          bench.end(n);
          break;
        case 2:
          bench.start();
          for (let i = 0; i < n; i++) method(command, options).kill();
          bench.end(n);
          break;
        case 3:
          bench.start();
          for (let i = 0; i < n; i++) method(command, options, cb).kill();
          bench.end(n);
          break;
      }
      break;
    case 'execSync':
      switch (params) {
        case 1:
          bench.start();
          for (let i = 0; i < n; i++) method(command);
          bench.end(n);
          break;
        case 2:
          bench.start();
          for (let i = 0; i < n; i++) method(command, options);
          bench.end(n);
          break;
      }
      break;
    case 'execFile':
      switch (params) {
        case 1:
          bench.start();
          for (let i = 0; i < n; i++) method(command).kill();
          bench.end(n);
          break;
        case 2:
          bench.start();
          for (let i = 0; i < n; i++) method(command, args).kill();
          bench.end(n);
          break;
        case 3:
          bench.start();
          for (let i = 0; i < n; i++) method(command, args, options).kill();
          bench.end(n);
          break;
        case 4:
          bench.start();
          for (let i = 0; i < n; i++) method(command, args, options, cb).kill();
          bench.end(n);
          break;
      }
      break;
    case 'execFileSync':
    case 'spawnSync':
      switch (params) {
        case 1:
          bench.start();
          for (let i = 0; i < n; i++) method(command);
          bench.end(n);
          break;
        case 2:
          bench.start();
          for (let i = 0; i < n; i++) method(command, args);
          bench.end(n);
          break;
        case 3:
          bench.start();
          for (let i = 0; i < n; i++) method(command, args, options);
          bench.end(n);
          break;
      }
      break;
    case 'spawn':
      switch (params) {
        case 1:
          bench.start();
          for (let i = 0; i < n; i++) method(command).kill();
          bench.end(n);
          break;
        case 2:
          bench.start();
          for (let i = 0; i < n; i++) method(command, args).kill();
          bench.end(n);
          break;
        case 3:
          bench.start();
          for (let i = 0; i < n; i++) method(command, args, options).kill();
          bench.end(n);
          break;
      }
      break;
  }
}
