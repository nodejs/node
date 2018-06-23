'use strict'

module.exports = strong

var all = require('../all')

/* Transform importance. */
function strong(h, node) {
  return h(node, 'strong', all(h, node))
}
