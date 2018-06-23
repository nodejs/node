'use strict'

module.exports = emphasis

var all = require('../all')

/* Transform emphasis. */
function emphasis(h, node) {
  return h(node, 'em', all(h, node))
}
