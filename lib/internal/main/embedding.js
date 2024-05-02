'use strict';
const {
  prepareMainThreadExecution,
} = require('internal/process/pre_execution');
const { isExperimentalSeaWarningNeeded } = internalBinding('sea');
const { emitExperimentalWarning } = require('internal/util');
const { embedderRequire, embedderRunCjs } = require('internal/util/embedding');

prepareMainThreadExecution(false, true);

if (isExperimentalSeaWarningNeeded()) {
  emitExperimentalWarning('Single executable application');
}

return [process, embedderRequire, embedderRunCjs];
