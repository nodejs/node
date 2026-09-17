'use strict';

const { Database, Statement, Session, constants, backup } =
  internalBinding('sqlite');

module.exports = {
  Database,
  // DEP0210: Documentation-only deprecation, kept for backward
  // compatibility with the class's pre-rename name.
  DatabaseSync: Database,
  Statement,
  // DEP0211: Documentation-only deprecation, kept for backward
  // compatibility with the class's pre-rename name.
  StatementSync: Statement,
  Session,
  constants,
  backup,
};
