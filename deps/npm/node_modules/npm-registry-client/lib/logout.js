module.exports = logout

var assert = require("assert")
var url = require("url")

function logout (uri, params, cb) {
  assert(typeof uri === "string", "must pass registry URI to logout")
  assert(params && typeof params === "object", "must pass params to logout")
  assert(typeof cb === "function", "must pass callback to star")

  var auth = params.auth
  assert(auth && typeof auth === "object", "must pass auth to logout")
  assert(typeof auth.token === "string", "can only log out for token auth")

  uri = url.resolve(uri, "-/user/token/" + auth.token)
  var options = {
    method: "DELETE",
    auth: auth
  }

  this.log.verbose("logout", "invalidating session token for user")
  this.request(uri, options, cb)
}
