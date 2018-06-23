'use strict'

module.exports = root

var u = require('unist-builder')
var wrap = require('../wrap')
var all = require('../all')

/* Transform a `root`. */
function root(h, node) {
  return h.augment(node, u('root', wrap(all(h, node))))
}
