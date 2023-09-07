'use strict';

const { defaultLoad } = require('internal/modules/esm/load');
const { defaultResolve } = require('internal/modules/esm/resolve');

exports.resolve = defaultResolve;
exports.load = defaultLoad;
