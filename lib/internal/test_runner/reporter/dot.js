'use strict';

const { green, red, clear } = require('internal/util/colors');
const { MathMax } = primordials;

module.exports = async function* dot(source) {
  let count = 0;
  let columns = getLineLength();
  const shouldColorize = process.stdout.isTTY || process.env.FORCE_COLOR !== undefined;
  for await (const { type } of source) {
    if (type === 'test:pass') {
      // Green dot for passing tests
      yield shouldColorize ? `${green}.${clear}` : '.';
    }
    if (type === 'test:fail') {
      // Red dot for failing tests
      yield shouldColorize ? `${red}X${clear}` : 'X';
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
