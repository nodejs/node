'use strict'

var Promise = require('pinkie-promise')
var runner = require('./runner')
var schemas = require('./schemas')

var promisify = function (schema) {
  return function (data) {
    return new Promise(function (resolve, reject) {
      runner(schema, data, function (err, valid) {
        return err === null ? resolve(data) : reject(err)
      })
    })
  }
}

module.exports = promisify(schemas.har)

// utility methods for all parts of the schema
Object.keys(schemas).map(function (name) {
  module.exports[name] = promisify(schemas[name])
})
