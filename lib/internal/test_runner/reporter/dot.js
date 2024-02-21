'use strict';

const { green, red, clear } = require('internal/util/colors');
const { MathMax } = primordials;

module.exports = async function* dot(source) {
  let count = 0;
  let columns = getLineLength();

  for await (const { type } of source) {
    if (type === 'test:pass') {
      // Green dot for passing tests
      yield `${green}.${clear}`;
    }

    if (type === 'test:fail') {
      // Red dot for failing tests
      yield `${red}X${clear}`;
    }

    if ((type === 'test:fail' || type === 'test:pass') && ++count === columns) {
      // Getting the line length again in case the terminal was resized.
      columns = getLineLength();
      count = 0;
    }
  }

  yield '\n';
};

function getLineLength() {
  return MathMax(process.stdout.columns ?? 20, 20);
}
