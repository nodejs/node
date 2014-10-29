
module.exports = star

function star (uri, starred, cb) {
  var c = this.conf.getCredentialsByURI(uri)
  if (c.token) {
    return cb(new Error("This operation is unsupported for token-based auth"))
  }
  else if (!c.auth) {
    return cb(new Error("Must be logged in to star/unstar packages"))
  }

  this.request("GET", uri + "?write=true", null, function (er, fullData) {
    if (er) return cb(er)

    fullData = { _id: fullData._id
               , _rev: fullData._rev
               , users: fullData.users || {} }

    if (starred) {
      this.log.info("starring", fullData._id)
      fullData.users[c.username] = true
      this.log.verbose("starring", fullData)
    } else {
      delete fullData.users[c.username]
      this.log.info("unstarring", fullData._id)
      this.log.verbose("unstarring", fullData)
    }

    return this.request("PUT", uri, { body : fullData }, cb)
  }.bind(this))
}
