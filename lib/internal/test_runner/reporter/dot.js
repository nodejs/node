'use strict';

module.exports = async function* dot(source) {
  let count = 0;
  for await (const { type } of source) {
    if (type === 'test:pass') {
      yield '.';
    }
    if (type === 'test:fail') {
      yield 'X';
    }
    if ((type === 'test:fail' || type === 'test:pass') && ++count % 20 === 0) {
      yield '\n';
    }
  }
  yield '\n';
};
