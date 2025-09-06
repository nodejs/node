'use strict';
const {
  ArrayPrototypePush,
  MathMax,
  SymbolAsyncIterator,
} = primordials;
const colors = require('internal/util/colors');
const { formatTestReport } = require('internal/test_runner/reporter/utils');

module.exports = async function* dot(source) {
  let count = 0;
  let columns = getLineLength();
  const failedTests = [];
  const sourceIterator = source[SymbolAsyncIterator]();
  let sourceResult = await sourceIterator.next();
  while (!sourceResult.done) {
    const { type, data } = sourceResult.value;
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
    sourceResult = await sourceIterator.next();
  }
  yield '\n';
  if (failedTests.length > 0) {
    yield `\n${colors.red}Failed tests:${colors.white}\n\n`;
    for (let i = 0; i < failedTests.length; i++) {
      const test = failedTests[i];
      yield formatTestReport('test:fail', test);
    }
  }
};

function getLineLength() {
  return MathMax(process.stdout.columns ?? 20, 20);
}
