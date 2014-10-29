module.exports = whoami

var url = require("url")

function whoami (uri, cb) {
  if (!this.conf.getCredentialsByURI(uri)) {
    return cb(new Error("Must be logged in to see who you are"))
  }

  this.request("GET", url.resolve(uri, "whoami"), null, function (er, userdata) {
    if (er) return cb(er)

    cb(null, userdata.username)
  })
}
