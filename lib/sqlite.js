'use strict';

const {
  Database,
  StatementSync,
  Session,
  constants,
  backup,
} = internalBinding('sqlite');

module.exports = {
  Database,
  // DEP0210: Documentation-only deprecation, kept for backward
  // compatibility with the class's pre-rename name.
  DatabaseSync: Database,
  StatementSync,
  Session,
  constants,
  backup,
};
