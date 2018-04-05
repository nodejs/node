'use strict'
module.exports = isRegistry

function isRegistry (req) {
  if (req == null) return false
  // modern metadata
  if ('registry' in req) return req.registry
  // legacy metadata
  if (req.type === 'range' || req.type === 'version' || req.type === 'tag') return true
  return false
}

