'use strict';
const { emitExperimentalWarning } = require('internal/util');

emitExperimentalWarning('SQLite');

const cppBinding = internalBinding('sqlite');

function processSqlTemplate(strings, ...values) {
  if (!strings || !Array.isArray(strings) || !strings.raw) {
    throw new TypeError('Invalid template strings array');
  }

  let sql = strings.raw[0];
  const params = [];

  for (let i = 0; i < values.length; i++) {
    sql += '?' + strings.raw[i + 1];
    params.push(values[i]);
  }

  return { sql, params };
}

function createSqlTag(nativeDb) {
  if (!nativeDb || typeof nativeDb.prepare !== 'function') {
    throw new TypeError('Invalid native database object provided for createSqlTag.');
  }

  return function sqlTemplate(strings, ...values) {
    const { sql, params } = processSqlTemplate(strings, ...values);
    const statement = nativeDb.prepare(sql);

    
    return statement.all(...params);
  };
}

module.exports = {
  createSqlTag,
  processSqlTemplate,
  ...cppBinding,
};