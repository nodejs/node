'use strict';

module.exports = async function * customReporter() {
  yield 'Going to throw an error\n';
  throw new Error('Reporting error');
};
