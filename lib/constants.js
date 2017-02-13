// Copyright Node.js contributors. All rights reserved.
// SPDX-License-Identifier: MIT

'use strict';

// This module is deprecated in documentation only. Users should be directed
// towards using the specific constants exposed by the individual modules on
// which they are most relevant.
// Deprecation Code: DEP0008
const constants = process.binding('constants');
Object.assign(exports,
              constants.os.errno,
              constants.os.signals,
              constants.fs,
              constants.crypto);
