'use strict'
var isDev = exports.isDev = function (node) {
  return node.package._requiredBy.some(function (req) { return req === '#DEV:/' })
}
exports.isOnlyDev = function (node) {
  return node.package._requiredBy.length === 1 && isDev(node)
}
