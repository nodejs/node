'use strict'

var validator = require('is-my-json-valid')
var schemas = require('./schemas.json')

function ValidationError (errors) {
  this.name = 'ValidationError'
  this.errors = errors
}

ValidationError.prototype = Error.prototype

var runner = function (schema, data, cb) {
  var validate = validator(schema, {
    greedy: true,
    verbose: true,
    schemas: schemas
  })

  var valid = false

  if (data !== undefined) {
    // execute is-my-json-valid
    valid = validate(data)
  }

  // callback?
  if (!cb) {
    return validate.errors ? false : true
  } else {
    return cb(validate.errors ? new ValidationError(validate.errors) : null, valid)
  }

  return valid
}

module.exports = function (data, cb) {
  return runner(schemas.log, data, cb)
}

Object.keys(schemas).map(function (name) {
  module.exports[name] = function (data, cb) {
    return runner(schemas[name], data, cb)
  }
})
