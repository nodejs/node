// Flags: --expose-brotli
'use strict';

const common = require('../common');

common.expectWarning('ExperimentalWarning',
                     'The brotli API is experimental',
                     common.noWarnCode);

require('brotli');
