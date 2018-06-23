'use strict'

module.exports = linkReference

var normalize = require('mdurl/encode')
var failsafe = require('../failsafe')
var all = require('../all')

/* Transform a reference to a link. */
function linkReference(h, node) {
  var def = h.definition(node.identifier)
  var props = {href: normalize((def && def.url) || '')}

  if (def && def.title !== null && def.title !== undefined) {
    props.title = def.title
  }

  return failsafe(h, node, def) || h(node, 'a', props, all(h, node))
}
