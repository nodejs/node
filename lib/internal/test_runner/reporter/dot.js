'use strict';

const { green, red } = require('internal/util/colors');
const { MathMax } = primordials;

module.exports = async function* dot(source) {
  let count = 0;
  let columns = getLineLength();

  for await (const { type } of source) {
    if (type === 'test:pass') {
      // Made green dot for pass
      yield green('.');
    }

    if (type === 'test:fail') {
      // Made red dot for fail
      yield red('X');
    }

    if ((type === 'test:fail' || type === 'test:pass') && ++count === columns) {
	  yield '\n';
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
