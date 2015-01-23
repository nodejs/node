module.exports = whoami

var url = require("url")
  , assert = require("assert")

function whoami (uri, params, cb) {
  assert(typeof uri === "string", "must pass registry URI to whoami")
  assert(params && typeof params === "object", "must pass params to whoami")
  assert(typeof cb === "function", "must pass callback to whoami")

  var auth = params.auth
  assert(auth && typeof auth === "object", "must pass auth to whoami")

  if (auth.username) return process.nextTick(cb.bind(this, null, auth.username))

  this.request(url.resolve(uri, "-/whoami"), { auth : auth }, function (er, userdata) {
    if (er) return cb(er)

    cb(null, userdata.username)
  })
}
