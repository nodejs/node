'use strict';

const { deprecate } = require('internal/util');

const {
  Database,
  StatementSync,
  Session,
  constants,
  backup,
} = internalBinding('sqlite');

module.exports = {
  Database,
  DatabaseSync: deprecate(
    Database,
    'sqlite.DatabaseSync is deprecated, use sqlite.Database instead.',
    'DEP0210'),
  StatementSync,
  Session,
  constants,
  backup,
};
