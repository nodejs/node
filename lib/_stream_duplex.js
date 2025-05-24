'use strict';

module.exports = require('stream').Duplex;

process.emitWarning('The _stream_duplex module is deprecated. Use `node:stream` instead.',
                    'DeprecationWarning', 'DEP0193');
