'use strict';
// This module tests transitive requires - it requires math.js using a
// relative path, which tests that module hooks resolve correctly.
const math = require('./math.js');

module.exports = {
  sum: (a, b) => math.add(a, b),
  product: (a, b) => math.multiply(a, b),
};
