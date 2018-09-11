/* eslint-disable node-core/required-modules */

'use strict';

const {
  statSync,
  readFileSync,
  readdirSync,
} = require('fs');
const { fork } = require('child_process');
const { runInThisContext } = require('vm');
const path = require('path');
const { performance } = require('perf_hooks');

const BASE = `${__dirname}/fixtures/web-platform-tests`;

const target = process.argv[2];
const runTap = target === '--tap';

const tests = [
  'streams/',
  'streams/readable-streams/',
  'streams/writable-streams/',
  'streams/transform-streams/',
  'streams/piping/',
  '!streams/generate-test-wrappers.js',
];

let passed = 0;
let failed = 0;
let total = 0;

const start = Date.now();

function updateUI(name = '') {
  if (runTap) {
    return;
  }

  const dt = Math.floor(Date.now() - start) / 1000;
  const m = Math.floor(dt / 60).toString().padStart(2, '0');
  const s = Math.round(dt % 60).toString().padStart(2, '0');

  const line =
    `WPT [${m}:${s} | PASS ${passed} | FAIL ${failed}] ${name.trim()}`.trim();

  process.stdout.clearLine();
  process.stdout.cursorTo(0);
  process.stdout.write(line.length > 80 ? line.slice(0, 76) + '...' : line);
}

function runTest(test) {
  return new Promise((resolve, reject) => {
    const child = fork(__filename, [test], {
      execArgv: [
        '--no-warnings',
        '--expose-gc',
      ],
    });
    child.on('message', ({ pass, fail }) => {
      if (pass) {
        passed++;
        if (runTap) {
          console.log(`ok ${total} ${pass.test}`);
        }
      } else if (fail) {
        failed++;
        const { stack, reason, test } = fail;
        if (runTap) {
          console.log(`not ok ${total} ${test}`);
          console.log(`  ---
  severity: fail
  stack |-
    ${stack.split('\n').join('\n    ')}
  ...`);
        } else {
          process.exitCode = 1;
          console.error(`\n\u00D7 ${test} (${reason})`);
          console.error(stack);
        }
      }
      total++;
      updateUI((pass || fail).test);
    });
    child.on('exit', () => {
      resolve();
    });
  });
}

if (!target || runTap) {
  if (runTap) {
    console.log('TAP version 13');
  } else {
    process.stdout.write('Finding tests...');
  }

  const queue = new Set();

  tests.forEach((s) => {
    if (s.startsWith('!')) {
      const key = `${BASE}/${s.slice(1)}`;
      queue.delete(key);
      return;
    }

    const p = `${BASE}/${s}`;
    const stat = statSync(p);
    if (stat.isDirectory()) {
      readdirSync(p)
        .filter((n) => n.endsWith('.js'))
        .forEach((n) => queue.add(`${p}${n}`));
    } else {
      queue.add(p);
    }
  });

  const a = [...queue];

  if (!runTap) {
    console.log(' Done.');
  }

  updateUI();

  runTest(a.shift()).then(function t() {
    const test = a.shift();
    if (!test) {
      return;
    }
    return runTest(test).then(t);
  }).then(() => {
    updateUI();
    process.stdout.write('\n');
  });
} else {
  global.importScripts = (s) => {
    const p = path.isAbsolute(s) ?
      `${BASE}${s}` :
      path.resolve(path.dirname(target), s);
    if (p === `${BASE}/resources/testharness.js`) {
      return;
    }
    const source = readFileSync(p, 'utf8');
    runInThisContext(source, { filename: p });
  };

  global.performance = performance;

  process.on('unhandledRejection', () => 0);

  function pass(test) {
    if (process.send) {
      process.send({ pass: { test: test.name } });
    } else {
      console.log(`✓ ${test.name}`);
    }
  }

  function fail({ test, reason }) {
    if (process.send) {
      process.send({ fail: {
        test: test.name,
        reason,
        stack: test.stack,
      } });
    } else {
      console.error(`\u00D7 ${test.name} (${reason})`.trim());
      const err = new Error(test.message);
      err.stack = test.stack;
      console.error(err);
    }
  }

  {
    const harness = `${BASE}/resources/testharness.js`;
    const source = readFileSync(harness, 'utf8');
    runInThisContext(source, { filename: harness });
  }

  global.add_result_callback((test) => {
    if (test.status === 1) {
      fail({ test, reason: 'failure' });
    } else if (test.status === 2) {
      fail({ test, reason: 'timeout' });
    } else if (test.status === 3) {
      fail({ test, reason: 'incomplete' });
    } else {
      pass(test);
    }
  });

  global.add_completion_callback((tests, harnessStatus) => {
    if (harnessStatus.status === 2) {
      fail({
        test: {
          message: 'test harness should not timeout',
        },
        reason: 'timeout',
      });
    }
  });

  // assign global.self late to trick wpt into
  // thinking this is a shell environment
  global.self = global;

  const source = readFileSync(target, 'utf8');
  runInThisContext(source, { filename: target });
}
