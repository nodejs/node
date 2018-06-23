'use strict'

module.exports = hardBreak

var u = require('unist-builder')

/* Transform an inline break. */
function hardBreak(h, node) {
  return [h(node, 'br'), u('text', '\n')]
}
