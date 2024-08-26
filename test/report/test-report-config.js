// Flags: --report-on-fatalerror --report-on-signal --report-uncaught-exception --report-compact
'use strict';
const common = require('../common');
const assert = require('assert');
const { test } = require('node:test');

const invalidArgType = { code: 'ERR_INVALID_ARG_TYPE' };
const cases = [
  {
    key: 'directory',
    value: '',
    newValue: __dirname,
    throwingSetters: [[{}, invalidArgType]],
  },
  {
    key: 'filename',
    value: '',
    newValue: 'test-report.json',
    throwingSetters: [[{}, invalidArgType]],
  },
  {
    key: 'reportOnFatalError',
    value: true,
    newValue: false,
    throwingSetters: [[{}, invalidArgType]],
  },
  {
    key: 'reportOnUncaughtException',
    value: true,
    newValue: false,
    throwingSetters: [[{}, invalidArgType]],
  },
  {
    key: 'reportOnSignal',
    value: false,
    newValue: true,
    throwingSetters: [[{}, invalidArgType]],
  },
  {
    key: 'compact',
    value: true,
    newValue: false,
    throwingSetters: [[{}, invalidArgType]],
  },
  {
    key: 'excludeNetwork',
    value: false,
    newValue: true,
    throwingSetters: [[{}, invalidArgType]],
  },
];

if (!common.isWindows) {
  cases.push({
    key: 'signal',
    value: 'SIGUSR1',
    newValue: 'SIGUSR2',
    throwingSetters: [
      [{}, invalidArgType],
      ['foo', { code: 'ERR_UNKNOWN_SIGNAL', message: 'Unknown signal: foo' }],
      ['sigusr1', { code: 'ERR_UNKNOWN_SIGNAL',
                    message: 'Unknown signal: sigusr1 (signals must use all capital letters)' }],
    ],
  });

  test('Verify that the interaction between reportOnSignal and signal is correct.', (t) => {
    process.report.reportOnSignal = false;
    t.assert.strictEqual(process.listenerCount('SIGUSR2'), 0);

    process.report.reportOnSignal = true;
    t.assert.strictEqual(process.listenerCount('SIGUSR2'), 1);

    process.report.signal = 'SIGUSR1';
    t.assert.strictEqual(process.listenerCount('SIGUSR2'), 0);
    t.assert.strictEqual(process.listenerCount('SIGUSR1'), 1);

    process.report.reportOnSignal = false;
    t.assert.strictEqual(process.listenerCount('SIGUSR1'), 0);
  });
}

for (const testCase of cases) {
  const { key, value, newValue, throwingSetters } = testCase;

  test(`Verify that process.report.${key} behaves properly`, (t) => {
    t.assert.strictEqual(process.report[key], value);

    process.report[key] = newValue;
    t.assert.strictEqual(process.report[key], newValue);

    for (const throwingSetter of throwingSetters) {
      assert.throws(() => process.report[key] = throwingSetter[0], throwingSetter[1]);
    }

    process.report[key] = newValue;
    t.assert.strictEqual(process.report[key], newValue);
  });
}
