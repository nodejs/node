'use strict';
require('../common');
const assert = require('assert');
const helper = require('../common/report');

{
  // Test with no arguments.
  helper.validateContent(process.report.getReport());
  assert.deepStrictEqual(helper.findReports(process.pid, process.cwd()), []);
}

{
  // Test with an error argument.
  helper.validateContent(process.report.getReport(new Error('test error')));
  assert.deepStrictEqual(helper.findReports(process.pid, process.cwd()), []);
}

{
  // Test with an error with one line stack
  const error = new Error();
  error.stack = 'only one line';
  helper.validateContent(process.report.getReport(error));
  assert.deepStrictEqual(helper.findReports(process.pid, process.cwd()), []);
}

// Test with an invalid error argument.
[null, 1, Symbol(), function() {}, 'foo'].forEach((error) => {
  assert.throws(() => {
    process.report.getReport(error);
  }, { code: 'ERR_INVALID_ARG_TYPE' });
});
