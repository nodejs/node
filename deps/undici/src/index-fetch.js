'use strict'

const fetchImpl = require('./lib/web/fetch').fetch

module.exports.fetch = function fetch (resource, init = undefined) {
  return fetchImpl(resource, init).catch((err) => {
    if (err && typeof err === 'object') {
      Error.captureStackTrace(err, this)
    }
    throw err
  })
}
module.exports.FormData = require('./lib/web/fetch/formdata').FormData
module.exports.Headers = require('./lib/web/fetch/headers').Headers
module.exports.Response = require('./lib/web/fetch/response').Response
module.exports.Request = require('./lib/web/fetch/request').Request

module.exports.WebSocket = require('./lib/web/websocket/websocket').WebSocket
module.exports.MessageEvent = require('./lib/web/websocket/events').MessageEvent

module.exports.EventSource = require('./lib/web/eventsource/eventsource').EventSource
