'use strict'

module.exports = thematicBreak

/* Transform a thematic break / horizontal rule. */
function thematicBreak(h, node) {
  return h(node, 'hr')
}
