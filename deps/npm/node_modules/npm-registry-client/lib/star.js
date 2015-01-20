module.exports = star

var assert = require("assert")

function star (uri, params, cb) {
  assert(typeof uri === "string", "must pass registry URI to star")
  assert(params && typeof params === "object", "must pass params to star")
  assert(typeof cb === "function", "must pass callback to star")

  var starred = params.starred ? true : false

  var auth = params.auth
  assert(auth && typeof auth === "object", "must pass auth to star")
  if (auth.token) {
    return cb(new Error("This operation is unsupported for token-based auth"))
  }
  else if (!(auth.username && auth.password)) {
    return cb(new Error("Must be logged in to star/unstar packages"))
  }

  var client = this
  this.request(uri+"?write=true", { auth : auth }, function (er, fullData) {
    if (er) return cb(er)

    fullData = {
      _id   : fullData._id,
      _rev  : fullData._rev,
      users : fullData.users || {}
    }

    if (starred) {
      client.log.info("starring", fullData._id)
      fullData.users[auth.username] = true
      client.log.verbose("starring", fullData)
    } else {
      delete fullData.users[auth.username]
      client.log.info("unstarring", fullData._id)
      client.log.verbose("unstarring", fullData)
    }

    var options = {
      method : "PUT",
      body : fullData,
      auth : auth
    }
    return client.request(uri, options, cb)
  })
}
