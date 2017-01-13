module.exports = send

var assert = require('assert')
var url = require('url')

function send (registryUrl, params, cb) {
  assert(typeof registryUrl === 'string', 'must pass registry URI')
  assert(params && typeof params === 'object', 'must pass params')
  assert(typeof cb === 'function', 'must pass callback')

  var uri = url.resolve(registryUrl, '-/npm/anon-metrics/v1/' +
    encodeURIComponent(params.metricId))

  this.request(uri, {
    method: 'PUT',
    body: JSON.stringify(params.metrics),
    authed: false
  }, cb)
}
