
module.exports = deprecate

var semver = require("semver")

function deprecate (name, ver, message, cb) {
  if (!this.conf.get('username')) {
    return cb(new Error("Must be logged in to deprecate a package"))
  }

  if (semver.validRange(ver) === null) {
    return cb(new Error("invalid version range: "+ver))
  }

  var users = {}

  this.get(name + '?write=true', function (er, data) {
    if (er) return cb(er)
    // filter all the versions that match
    Object.keys(data.versions).filter(function (v) {
      return semver.satisfies(v, ver)
    }).forEach(function (v) {
      data.versions[v].deprecated = message
    })
    // now update the doc on the registry
    this.request('PUT', data._id, data, cb)
  }.bind(this))
}
