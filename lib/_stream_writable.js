'use strict';

process.emitWarning(`The ${__filename} module is deprecated. Please use stream`,
                    'DeprecationWarning', 'DEP00XX');

module.exports = require('internal/streams/writable');
