'use strict';

module.exports = async function*(source) {
  let count = 0;
  for await (const { type } of source) {
    if (type === 'test:fail' || type === 'test:pass') {
      yield '.';
      if (++count % 20 === 0) {
        yield '\n';
      }
    }
  }
  yield '\n';
};
