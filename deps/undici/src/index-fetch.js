'use strict'

const { getGlobalDispatcher } = require('./lib/global')
const fetchImpl = require('./lib/fetch')

module.exports.fetch = async function fetch (resource) {
  const dispatcher = (arguments[1] && arguments[1].dispatcher) || getGlobalDispatcher()
  return fetchImpl.apply(dispatcher, arguments)
}
module.exports.FormData = require('./lib/fetch/formdata').FormData
module.exports.Headers = require('./lib/fetch/headers').Headers
module.exports.Response = require('./lib/fetch/response').Response
module.exports.Request = require('./lib/fetch/request').Request
