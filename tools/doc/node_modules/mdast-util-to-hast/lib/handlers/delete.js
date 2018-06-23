'use strict'

module.exports = strikethrough

var all = require('../all')

/* Transform deletions. */
function strikethrough(h, node) {
  return h(node, 'del', all(h, node))
}
