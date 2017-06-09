module.exports = star

var assert = require('assert')

function star (uri, params, cb) {
  assert(typeof uri === 'string', 'must pass registry URI to star')
  assert(params && typeof params === 'object', 'must pass params to star')
  assert(typeof cb === 'function', 'must pass callback to star')

  var starred = !!params.starred

  var auth = params.auth
  assert(auth && typeof auth === 'object', 'must pass auth to star')
  if (!(auth.token || (auth.password && auth.username && auth.email))) {
    var er = new Error('Must be logged in to star/unstar packages')
    er.code = 'ENEEDAUTH'
    return cb(er)
  }

  var client = this
  this.request(uri + '?write=true', { auth: auth }, function (er, fullData) {
    if (er) return cb(er)

    client.whoami(uri, params, function (er, username) {
      if (er) return cb(er)

      var data = {
        _id: fullData._id,
        _rev: fullData._rev,
        users: fullData.users || {}
      }

      if (starred) {
        client.log.info('starring', data._id)
        data.users[username] = true
        client.log.verbose('starring', data)
      } else {
        delete data.users[username]
        client.log.info('unstarring', data._id)
        client.log.verbose('unstarring', data)
      }

      var options = {
        method: 'PUT',
        body: data,
        auth: auth
      }
      return client.request(uri, options, cb)
    })
  })
}
