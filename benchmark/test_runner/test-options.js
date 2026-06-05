'use strict';

const common = require('../common');
const { finished } = require('node:stream/promises');
const reporter = require('../fixtures/empty-test-reporter');
const { it } = require('node:test');

const bench = common.createBenchmark(main, {
  n: [100, 1000],
  option: [
    'none',
    'skip',
    'skip-with-message',
    'skip-method',
    'skip-method-with-message',
    'todo',
    'todo-with-message',
    'todo-method',
    'todo-method-with-message',
  ],
}, {
  // We don't want to test the reporter here.
  flags: ['--test-reporter=./benchmark/fixtures/empty-test-reporter.js'],
});

async function run({ n, option }) {
  // eslint-disable-next-line no-unused-vars
  let avoidV8Optimization;

  for (let i = 0; i < n; i++) {
    switch (option) {
      case 'none':
        it(`${i}`, () => {
          avoidV8Optimization = i;
        });
        break;
      case 'skip':
        it(`${i}`, { skip: true }, () => {
          throw new Error('This test should not run.');
        });
        break;
      case 'skip-with-message':
        it(`${i}`, { skip: 'skip reason' }, () => {
          throw new Error('This test should not run.');
        });
        break;
      case 'skip-method':
        it(`${i}`, (t) => {
          avoidV8Optimization = i;
          t.skip();
        });
        break;
      case 'skip-method-with-message':
        it(`${i}`, (t) => {
          avoidV8Optimization = i;
          t.skip('skip reason');
        });
        break;
      case 'todo':
        it(`${i}`, { todo: true }, () => {
          avoidV8Optimization = i;
        });
        break;
      case 'todo-with-message':
        it(`${i}`, { todo: 'todo reason' }, () => {
          avoidV8Optimization = i;
        });
        break;
      case 'todo-method':
        it(`${i}`, (t) => {
          avoidV8Optimization = i;
          t.todo();
        });
        break;
      case 'todo-method-with-message':
        it(`${i}`, (t) => {
          avoidV8Optimization = i;
          t.todo('todo reason');
        });
        break;
    }
  }

  await finished(reporter);
  return n;
}

function main(params) {
  bench.start();

  run(params).then((ops) => {
    bench.end(ops);
  });
}
