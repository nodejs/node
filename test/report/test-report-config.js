// Flags: --experimental-report --diagnostic-report-on-fatalerror --diagnostic-report-on-signal --diagnostic-report-uncaught-exception
'use strict';
const common = require('../common');
common.skipIfReportDisabled();
const assert = require('assert');

common.expectWarning('ExperimentalWarning',
                     'report is an experimental feature. This feature could ' +
                     'change at any time');

// Verify that process.report.directory behaves properly.
assert.strictEqual(process.report.directory, '');
process.report.directory = __dirname;
assert.strictEqual(process.report.directory, __dirname);
common.expectsError(() => {
  process.report.directory = {};
}, { code: 'ERR_INVALID_ARG_TYPE' });
assert.strictEqual(process.report.directory, __dirname);

// Verify that process.report.filename behaves properly.
assert.strictEqual(process.report.filename, '');
process.report.filename = 'test-report.json';
assert.strictEqual(process.report.filename, 'test-report.json');
common.expectsError(() => {
  process.report.filename = {};
}, { code: 'ERR_INVALID_ARG_TYPE' });
assert.strictEqual(process.report.filename, 'test-report.json');

// Verify that process.report.reportOnFatalError behaves properly.
assert.strictEqual(process.report.reportOnFatalError, true);
process.report.reportOnFatalError = false;
assert.strictEqual(process.report.reportOnFatalError, false);
process.report.reportOnFatalError = true;
assert.strictEqual(process.report.reportOnFatalError, true);
common.expectsError(() => {
  process.report.reportOnFatalError = {};
}, { code: 'ERR_INVALID_ARG_TYPE' });
assert.strictEqual(process.report.reportOnFatalError, true);


// Verify that process.report.reportOnUncaughtException behaves properly.
assert.strictEqual(process.report.reportOnUncaughtException, true);
process.report.reportOnUncaughtException = false;
assert.strictEqual(process.report.reportOnUncaughtException, false);
process.report.reportOnUncaughtException = true;
assert.strictEqual(process.report.reportOnUncaughtException, true);
common.expectsError(() => {
  process.report.reportOnUncaughtException = {};
}, { code: 'ERR_INVALID_ARG_TYPE' });
assert.strictEqual(process.report.reportOnUncaughtException, true);

// Verify that process.report.reportOnSignal behaves properly.
assert.strictEqual(process.report.reportOnSignal, true);
process.report.reportOnSignal = false;
assert.strictEqual(process.report.reportOnSignal, false);
process.report.reportOnSignal = true;
assert.strictEqual(process.report.reportOnSignal, true);
common.expectsError(() => {
  process.report.reportOnSignal = {};
}, { code: 'ERR_INVALID_ARG_TYPE' });
assert.strictEqual(process.report.reportOnSignal, true);

if (!common.isWindows) {
  // Verify that process.report.signal behaves properly.
  assert.strictEqual(process.report.signal, 'SIGUSR2');
  common.expectsError(() => {
    process.report.signal = {};
  }, { code: 'ERR_INVALID_ARG_TYPE' });
  common.expectsError(() => {
    process.report.signal = 'foo';
  }, { code: 'ERR_UNKNOWN_SIGNAL' });
  assert.strictEqual(process.report.signal, 'SIGUSR2');
  process.report.signal = 'SIGUSR1';
  assert.strictEqual(process.report.signal, 'SIGUSR1');

  // Verify that the interaction between reportOnSignal and signal is correct.
  process.report.signal = 'SIGUSR2';
  process.report.reportOnSignal = false;
  assert.strictEqual(process.listenerCount('SIGUSR2'), 0);
  process.report.reportOnSignal = true;
  assert.strictEqual(process.listenerCount('SIGUSR2'), 1);
  process.report.signal = 'SIGUSR1';
  assert.strictEqual(process.listenerCount('SIGUSR2'), 0);
  assert.strictEqual(process.listenerCount('SIGUSR1'), 1);
  process.report.reportOnSignal = false;
  assert.strictEqual(process.listenerCount('SIGUSR1'), 0);
}
