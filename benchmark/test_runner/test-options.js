'use strict';

const common = require('../common');
const { finished } = require('node:stream/promises');
const reporter = require('../fixtures/empty-test-reporter');
const { it } = require('node:test');

const bench = common.createBenchmark(main, {
  n: [10000],
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

const allTests = {
  'none': (loopAmount, avoidV8Optimization) => {
    for (let i = 0; i < loopAmount; i++) {
      it(`${i}`, () => {
        avoidV8Optimization = i;
      });
    }

    return finished(reporter);
  },
  'skip': (loopAmount) => {
    for (let i = 0; i < loopAmount; i++) {
      it(`${i}`, { skip: true }, () => {
        throw new Error('This test should not run.');
      });
    }

    return finished(reporter);
  },
  'skip-with-message': (loopAmount) => {
    for (let i = 0; i < loopAmount; i++) {
      it(`${i}`, { skip: 'skip reason' }, () => {
        throw new Error('This test should not run.');
      });
    }

    return finished(reporter);
  },
  'skip-method': (loopAmount, avoidV8Optimization) => {
    for (let i = 0; i < loopAmount; i++) {
      it(`${i}`, (t) => {
        avoidV8Optimization = i;
        t.skip();
      });
    }

    return finished(reporter);
  },
  'skip-method-with-message': (loopAmount, avoidV8Optimization) => {
    for (let i = 0; i < loopAmount; i++) {
      it(`${i}`, (t) => {
        avoidV8Optimization = i;
        t.skip('skip reason');
      });
    }

    return finished(reporter);
  },
  'todo': (loopAmount, avoidV8Optimization) => {
    for (let i = 0; i < loopAmount; i++) {
      it(`${i}`, { todo: true }, () => {
        avoidV8Optimization = i;
      });
    }

    return finished(reporter);
  },
  'todo-with-message': (loopAmount, avoidV8Optimization) => {
    for (let i = 0; i < loopAmount; i++) {
      it(`${i}`, { todo: 'todo reason' }, () => {
        avoidV8Optimization = i;
      });
    }

    return finished(reporter);
  },
  'todo-method': (loopAmount, avoidV8Optimization) => {
    for (let i = 0; i < loopAmount; i++) {
      it(`${i}`, (t) => {
        avoidV8Optimization = i;
        t.todo();
      });
    }

    return finished(reporter);
  },
  'todo-method-with-message': (loopAmount, avoidV8Optimization) => {
    for (let i = 0; i < loopAmount; i++) {
      it(`${i}`, (t) => {
        avoidV8Optimization = i;
        t.todo('todo reason');
      });
    }

    return finished(reporter);
  },
};

function main({ n, option }) {
  // eslint-disable-next-line prefer-const
  let avoidV8Optimization = 0;
  const runOption = allTests[option];

  bench.start();

  runOption(n, avoidV8Optimization).then(() => {
    bench.end(n);
  });
}
