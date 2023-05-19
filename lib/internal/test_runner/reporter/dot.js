'use strict';
const { MathMax } = primordials;

module.exports = async function* dot(source) {
  let count = 0;
  let columns = getLineLength();
  for await (const { type } of source) {
    if (type === 'test:pass') {
      yield '.';
    }
    if (type === 'test:fail') {
      yield 'X';
    }
    if ((type === 'test:fail' || type === 'test:pass') && ++count === columns) {
      yield '\n';

      // Getting again in case the terminal was resized.
      columns = getLineLength();
      count = 0;
    }
  }
  yield '\n';
};

function getLineLength() {
  return MathMax(process.stdout.columns ?? 20, 20);
}
