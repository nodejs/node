'use strict';

const { deprecate } = require('internal/util');

const sqlite = internalBinding('sqlite');

module.exports = {
  ...sqlite,
  DatabaseSync: deprecate(
    sqlite.Database,
    'sqlite.DatabaseSync is deprecated, use sqlite.Database instead.',
    'DEP0210'),
};
