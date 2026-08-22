'use strict';
const {
  ArrayPrototypePush,
  MathMax,
} = primordials;
const colors = require('internal/util/colors');
const { formatTestReport } = require('internal/test_runner/reporter/utils');

module.exports = async function* dot(source) {
  let count = 0;
  let columns = getLineLength();
  const failedTests = [];
  const diagnosticErrors = [];
  for await (const { type, data } of source) {
    if (type === 'test:pass') {
      yield `${colors.green}.${colors.reset}`;
    }
    if (type === 'test:fail') {
      yield `${colors.red}X${colors.reset}`;
      ArrayPrototypePush(failedTests, data);
    }
    if (type === 'test:diagnostic' && data.level === 'error') {
      ArrayPrototypePush(diagnosticErrors, data.message);
    }
    if ((type === 'test:fail' || type === 'test:pass') && ++count === columns) {
      yield '\n';

      // Getting again in case the terminal was resized.
      columns = getLineLength();
      count = 0;
    }
  }
  yield '\n';
  if (failedTests.length > 0) {
    yield `\n${colors.red}Failed tests:${colors.white}\n\n`;
    for (const test of failedTests) {
      yield formatTestReport('test:fail', test);
    }
  }
  if (diagnosticErrors.length > 0) {
    yield `\n${colors.red}Diagnostic errors:${colors.white}\n\n`;
    for (const msg of diagnosticErrors) {
      yield `${msg}\n`;
    }
  }
};

function getLineLength() {
  return MathMax(process.stdout.columns ?? 20, 20);
}
