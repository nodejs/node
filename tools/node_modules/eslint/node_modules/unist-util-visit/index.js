'use strict'

module.exports = visit

var is = require('unist-util-is')

var CONTINUE = true
var SKIP = 'skip'
var EXIT = false

visit.CONTINUE = CONTINUE
visit.SKIP = SKIP
visit.EXIT = EXIT

function visit(tree, test, visitor, reverse) {
  if (typeof test === 'function' && typeof visitor !== 'function') {
    reverse = visitor
    visitor = test
    test = null
  }

  one(tree)

  /* Visit a single node. */
  function one(node, index, parent) {
    var result

    index = index || (parent ? 0 : null)

    if (!test || node.type === test || is(test, node, index, parent || null)) {
      result = visitor(node, index, parent || null)
    }

    if (result === EXIT) {
      return result
    }

    if (node.children && result !== SKIP) {
      return all(node.children, node) === EXIT ? EXIT : result
    }

    return result
  }

  /* Visit children in `parent`. */
  function all(children, parent) {
    var step = reverse ? -1 : 1
    var index = (reverse ? children.length : -1) + step
    var child
    var result

    while (index > -1 && index < children.length) {
      child = children[index]
      result = child && one(child, index, parent)

      if (result === EXIT) {
        return result
      }

      index = typeof result === 'number' ? result : index + step
    }

    return CONTINUE
  }
}
