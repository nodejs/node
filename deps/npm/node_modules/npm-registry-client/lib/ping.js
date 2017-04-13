module.exports = ping

var url = require('url')
var assert = require('assert')

function ping (uri, params, cb) {
  assert(typeof uri === 'string', 'must pass registry URI to ping')
  assert(params && typeof params === 'object', 'must pass params to ping')
  assert(typeof cb === 'function', 'must pass callback to ping')

  var auth = params.auth
  assert(auth && typeof auth === 'object', 'must pass auth to ping')

  this.request(url.resolve(uri, '-/ping?write=true'), { auth: auth }, function (er, fullData, data, response) {
    if (er || fullData) {
      cb(er, fullData, data, response)
    } else {
      cb(new Error('No data received'))
    }
  })
}
