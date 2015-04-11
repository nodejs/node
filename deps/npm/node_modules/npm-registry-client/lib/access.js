module.exports = access

var assert = require('assert')

function access (uri, params, cb) {
  assert(typeof uri === 'string', 'must pass registry URI to access')
  assert(params && typeof params === 'object', 'must pass params to access')
  assert(typeof cb === 'function', 'muss pass callback to access')

  assert(typeof params.level === 'string', 'must pass level to access')
  assert(
    ['public', 'restricted'].indexOf(params.level) !== -1,
    "access level must be either 'public' or 'restricted'"
  )
  assert(
    params.auth && typeof params.auth === 'object',
    'must pass auth to access'
  )

  var body = {
    access: params.level
  }

  var options = {
    method: 'POST',
    body: JSON.stringify(body),
    auth: params.auth
  }
  this.request(uri, options, cb)
}
