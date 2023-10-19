'use strict';
const {
  prepareMainThreadExecution,
  markBootstrapComplete,
} = require('internal/process/pre_execution');
const { isExperimentalSeaWarningNeeded } = internalBinding('sea');
const { emitExperimentalWarning } = require('internal/util');
const { embedderRequire, embedderRunCjs } = require('internal/util/embedding');
const { runEmbedderEntryPoint } = internalBinding('mksnapshot');

prepareMainThreadExecution(false, true);
markBootstrapComplete();

if (isExperimentalSeaWarningNeeded()) {
  emitExperimentalWarning('Single executable application');
}

return runEmbedderEntryPoint(process, embedderRequire, embedderRunCjs);
