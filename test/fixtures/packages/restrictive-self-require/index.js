'use strict';
// Self-require of a subpath that is not listed in this package's
// `exports` map. `trySelf` resolves the bare specifier through
// `packageExportsResolve`, which throws ERR_PACKAGE_PATH_NOT_EXPORTED.
module.exports = require('restrictive-self-require/not-in-exports');
