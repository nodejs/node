// Copyright Node.js contributors. All rights reserved.
// SPDX-License-Identifier: MIT

'use strict';

// the sys module was renamed to 'util'.
// this shim remains to keep old programs working.
// sys is deprecated and shouldn't be used

module.exports = require('util');
process.emitWarning('sys is deprecated. Use util instead.',
                    'DeprecationWarning', 'DEP0025');
