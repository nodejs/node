'use strict';
const {
  ArrayPrototypePush,
  MathMax,
} = primordials;
const colors = require('internal/util/colors');
const { formatTestReport, getCoverageReport } = require('internal/test_runner/reporter/utils');

module.exports = async function* dot(source) {
  let count = 0;
  let columns = getLineLength();
  const failedTests = [];
  const coverageFailures = [];
  for await (const { type, data } of source) {
    if (type === 'test:pass') {
      yield `${colors.green}.${colors.reset}`;
    }
    if (type === 'test:fail') {
      yield `${colors.red}X${colors.reset}`;
      ArrayPrototypePush(failedTests, data);
    }
    if (type === 'test:coverage') {
      // Check if coverage failed (threshold not met)
      if (data.summary && (
        (data.summary.lines && data.summary.lines.pct < 100) ||
        (data.summary.functions && data.summary.functions.pct < 100) ||
        (data.summary.branches && data.summary.branches.pct < 100)
      )) {
        ArrayPrototypePush(coverageFailures, data);
      }
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
  if (coverageFailures.length > 0) {
    yield `\n${colors.red}Coverage report failed:${colors.white}\n\n`;
    for (const coverage of coverageFailures) {
      yield getCoverageReport('', coverage.summary, 'ℹ ', colors.blue, true);
    }
  }
};

function getLineLength() {
  return MathMax(process.stdout.columns ?? 20, 20);
}
