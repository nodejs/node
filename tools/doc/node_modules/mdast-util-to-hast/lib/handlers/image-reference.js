'use strict'

module.exports = imageReference

var normalize = require('mdurl/encode')
var failsafe = require('../failsafe')

/* Transform a reference to an image. */
function imageReference(h, node) {
  var def = h.definition(node.identifier)
  var props = {src: normalize((def && def.url) || ''), alt: node.alt}

  if (def && def.title !== null && def.title !== undefined) {
    props.title = def.title
  }

  return failsafe(h, node, def) || h(node, 'img', props)
}
