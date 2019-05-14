'use strict'

var visit = require('unist-util-visit')

module.exports = removePosition

/* Remove `position`s from `tree`. */
function removePosition(node, force) {
  visit(node, force ? hard : soft)
  return node
}

function hard(node) {
  delete node.position
}

function soft(node) {
  node.position = undefined
}
