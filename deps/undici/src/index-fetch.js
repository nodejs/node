'use strict'

const fetchImpl = require('./lib/fetch').fetch

module.exports.fetch = async function fetch (resource) {
  try {
    return await fetchImpl(...arguments)
  } catch (err) {
    Error.captureStackTrace(err, this)
    throw err
  }
}
module.exports.FormData = require('./lib/fetch/formdata').FormData
module.exports.Headers = require('./lib/fetch/headers').Headers
module.exports.Response = require('./lib/fetch/response').Response
module.exports.Request = require('./lib/fetch/request').Request
