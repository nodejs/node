'use strict'

var runner = require('./runner')
var schemas = require('./schemas')

module.exports = function (data, cb) {
  return runner(schemas.har, data, cb)
}

Object.keys(schemas).map(function (name) {
  module.exports[name] = function (data, cb) {
    return runner(schemas[name], data, cb)
  }
})
