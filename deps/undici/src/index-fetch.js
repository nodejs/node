'use strict'

const Agent = require('./lib/agent')

const globalDispatcher = new Agent()

const fetchImpl = require('./lib/fetch')
module.exports.fetch = async function fetch (resource) {
  return fetchImpl.apply(globalDispatcher, arguments)
}
module.exports.FormData = require('./lib/fetch/formdata').FormData
module.exports.Headers = require('./lib/fetch/headers').Headers
module.exports.Response = require('./lib/fetch/response').Response
module.exports.Request = require('./lib/fetch/request').Request
