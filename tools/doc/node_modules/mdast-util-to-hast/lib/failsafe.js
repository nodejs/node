'use strict'

module.exports = failsafe

var u = require('unist-builder')
var all = require('./all')

/* Return the content of a reference without definition
 * as markdown. */
function failsafe(h, node, definition) {
  var subtype = node.referenceType

  if (subtype !== 'collapsed' && subtype !== 'full' && !definition) {
    if (node.type === 'imageReference') {
      return u('text', '![' + node.alt + ']')
    }

    return [u('text', '[')].concat(all(h, node), u('text', ']'))
  }
}
