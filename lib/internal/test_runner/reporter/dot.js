'use strict';

const { MathMax } = primordials;

module.exports = async function* dot(source) {
  let count = 0;
  let columns = getLineLength();
  for await (const { type } of source) {
    if (type === 'test:pass') {
      // Print green dot for pass
      yield '\u001b[32m.\u001b[0m'; // \u001b[32m is ANSI escape code for green color
    }
    if (type === 'test:fail') {
      // Print red X for fail
      yield '\u001b[31mX\u001b[0m'; // \u001b[31m is ANSI escape code for red color
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
