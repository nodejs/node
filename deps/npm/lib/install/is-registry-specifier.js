'use strict'
module.exports = isRegistrySpecifier

function isRegistrySpecifier (spec) {
  return spec.type === 'range' || spec.type === 'version' || spec.type === 'tag'
}
