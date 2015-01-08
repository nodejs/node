module.exports = stars

var assert = require("assert")
var url = require("url")

function stars (uri, params, cb) {
  assert(typeof uri === "string", "must pass registry URI to stars")
  assert(params && typeof params === "object", "must pass params to stars")
  assert(typeof cb === "function", "must pass callback to stars")

  var auth = params.auth
  var name = params.username || (auth && auth.username)
  if (!name) return cb(new Error("must pass either username or auth to stars"))
  var encoded = encodeURIComponent(name)
  var path = "-/_view/starredByUser?key=\""+encoded+"\""

  this.request(url.resolve(uri, path), { auth : auth }, cb)
}
