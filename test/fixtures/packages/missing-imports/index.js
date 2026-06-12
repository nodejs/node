'use strict';
// Triggers the `#imports` resolution branch in Module._resolveFilename.
// The mapped target file does not exist, so finalizeEsmResolution throws
// a MODULE_NOT_FOUND error that the loader must enrich with requireStack.
module.exports = require('#missing');
