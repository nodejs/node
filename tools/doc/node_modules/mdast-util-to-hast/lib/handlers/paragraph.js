'use strict'

module.exports = paragraph

var all = require('../all')

/* Transform a paragraph. */
function paragraph(h, node) {
  return h(node, 'p', all(h, node))
}
