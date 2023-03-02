'use strict';
const {
  prepareMainThreadExecution,
  markBootstrapComplete,
} = require('internal/process/pre_execution');
const { isSea } = internalBinding('sea');
const { emitExperimentalWarning } = require('internal/util');
const { embedderRequire, embedderRunCjs } = require('internal/util/embedding');
const { getEmbedderEntryFunction } = internalBinding('mksnapshot');

prepareMainThreadExecution(false, true);
markBootstrapComplete();

if (isSea()) {
  emitExperimentalWarning('Single executable application');
}

return getEmbedderEntryFunction()(embedderRequire, embedderRunCjs);
