module.exports = deprecate

var assert = require("assert")
var url = require("url")
var semver = require("semver")

function deprecate (uri, params, cb) {
  assert(typeof uri === "string", "must pass registry URI to deprecate")
  assert(params && typeof params === "object", "must pass params to deprecate")
  assert(typeof cb === "function", "must pass callback to deprecate")

  assert(typeof params.version === "string", "must pass version to deprecate")
  assert(typeof params.message === "string", "must pass message to deprecate")
  assert(
    params.auth && typeof params.auth === "object",
    "must pass auth to deprecate"
  )

  var version = params.version
  var message = params.message
  var auth = params.auth

  if (semver.validRange(version) === null) {
    return cb(new Error("invalid version range: "+version))
  }

  this.get(uri + "?write=true", { auth : auth }, function (er, data) {
    if (er) return cb(er)
    // filter all the versions that match
    Object.keys(data.versions).filter(function (v) {
      return semver.satisfies(v, version)
    }).forEach(function (v) {
      data.versions[v].deprecated = message
    })
    // now update the doc on the registry
    var options = {
      method : "PUT",
      body   : data,
      auth   : auth
    }
    this.request(url.resolve(uri, data._id), options, cb)
  }.bind(this))
}
