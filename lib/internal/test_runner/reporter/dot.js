'use strict';
const {
  ArrayPrototypePush,
  MathMax,
} = primordials;
const colors = require('internal/util/colors');
const {
  formatTestReport,
  reporterColorMap,
  reporterUnicodeSymbolMap,
} = require('internal/test_runner/reporter/utils');

module.exports = async function* dot(source) {
  let count = 0;
  let columns = getLineLength();
  const failedTests = [];
  const diagnosticWarnings = [];
  for await (const { type, data } of source) {
    if (type === 'test:pass') {
      yield `${colors.green}.${colors.reset}`;
    }
    if (type === 'test:fail') {
      yield `${colors.red}X${colors.reset}`;
      ArrayPrototypePush(failedTests, data);
    }
    if (type === 'test:diagnostic' && (data.level === 'warn' || data.level === 'error')) {
      ArrayPrototypePush(diagnosticWarnings, data);
    }
    if ((type === 'test:fail' || type === 'test:pass') && ++count === columns) {
      yield '\n';

      // Getting again in case the terminal was resized.
      columns = getLineLength();
      count = 0;
    }
  }
  yield '\n';
  if (diagnosticWarnings.length > 0) {
    for (const diagnostic of diagnosticWarnings) {
      const diagnosticColor = reporterColorMap[diagnostic.level] || reporterColorMap['test:diagnostic'];
      yield `${diagnosticColor}${reporterUnicodeSymbolMap['test:diagnostic']}${diagnostic.message}${colors.white}\n`;
    }
  }
  if (failedTests.length > 0) {
    yield `\n${colors.red}Failed tests:${colors.white}\n\n`;
    for (const test of failedTests) {
      yield formatTestReport('test:fail', test);
    }
  }
};

function getLineLength() {
  return MathMax(process.stdout.columns ?? 20, 20);
}
