'use strict';
const {
  ArrayPrototypePush,
  MathMax,
} = primordials;
const colors = require('internal/util/colors');
const { getCoverageReport } = require('internal/test_runner/utils');
const {
  formatTestReport,
  reporterColorMap,
  reporterUnicodeSymbolMap,
} = require('internal/test_runner/reporter/utils');

module.exports = async function* dot(source) {
  let count = 0;
  let columns = getLineLength();
  const failedTests = [];
  const diagnostics = [];
  let coverage;
  for await (const { type, data } of source) {
    if (type === 'test:pass') {
      yield `${colors.green}.${colors.reset}`;
    }
    if (type === 'test:fail') {
      yield `${colors.red}X${colors.reset}`;
      ArrayPrototypePush(failedTests, data);
    }
    if ((type === 'test:fail' || type === 'test:pass') && ++count === columns) {
      yield '\n';

      // Getting again in case the terminal was resized.
      columns = getLineLength();
      count = 0;
    }
    if (type === 'test:diagnostic') {
      ArrayPrototypePush(diagnostics, data);
    }
    if (type === 'test:coverage') {
      coverage = data;
    }
  }
  yield '\n';
  if (failedTests.length > 0) {
    yield `\n${colors.red}Failed tests:${colors.white}\n\n`;
    for (const test of failedTests) {
      yield formatTestReport('test:fail', test);
    }
  }
  for (const diagnostic of diagnostics) {
    const color = reporterColorMap[diagnostic.level] || reporterColorMap['test:diagnostic'];
    yield `${color}${reporterUnicodeSymbolMap['test:diagnostic']}${diagnostic.message}${colors.white}\n`;
  }
  if (coverage) {
    yield getCoverageReport('', coverage.summary, reporterUnicodeSymbolMap['test:coverage'], colors.blue, true);
  }
};

function getLineLength() {
  return MathMax(process.stdout.columns ?? 20, 20);
}
