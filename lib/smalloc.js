'use strict';

const util = require('internal/util');

module.exports = require('internal/smalloc');
util.printDeprecationMessage('smalloc is deprecated. ' +
                             'Use typed arrays instead.');
