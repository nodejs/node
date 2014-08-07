
module.exports = bugs

function bugs (uri, cb) {
  this.get(uri + "/latest", 3600, function (er, d) {
    if (er) return cb(er)
    cb(null, d.bugs)
  })
}
