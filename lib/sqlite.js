'use strict';
const {
  globalThis,
} = primordials;

const { emitExperimentalWarning } = require('internal/util');

emitExperimentalWarning('SQLite');
module.exports = internalBinding('sqlite');

const { Iterator } = globalThis;
const statementIterate = module.exports.StatementSync.prototype.iterate;
module.exports.StatementSync.prototype.iterate = function iterate() {
  return Iterator.from(statementIterate.apply(this, arguments));
};
