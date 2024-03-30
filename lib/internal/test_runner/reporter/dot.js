'use strict';
const { MathMax } = primordials;
const { red, green, clear } = require('internal/util/colors');

module.exports = async function* dot(source) {
  let count = 0;
  let columns = getLineLength();
  for await (const { type } of source) {
    if (type === 'test:pass') {
      yield `${green}.${clear}`;
    }
    if (type === 'test:fail') {
      yield `${red}X${clear}`;
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
