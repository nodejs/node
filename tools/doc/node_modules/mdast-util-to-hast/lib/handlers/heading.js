'use strict'

module.exports = heading

var all = require('../all')

/* Transform a heading. */
function heading(h, node) {
  return h(node, 'h' + node.depth, all(h, node))
}
