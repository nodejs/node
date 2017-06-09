'use strict';

var value         = require('./valid-value')
  , stringifiable = require('./validate-stringifiable');

module.exports = function (x) { return stringifiable(value(x)); };
