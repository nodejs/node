'use strict'

module.exports = text

var u = require('unist-builder')
var trimLines = require('trim-lines')

/* Transform text. */
function text(h, node) {
  return h.augment(node, u('text', trimLines(node.value)))
}
