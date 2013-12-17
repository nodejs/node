
module.exports = bugs

function bugs (name, cb) {
  this.get(name + "/latest", 3600, function (er, d) {
    if (er) return cb(er)
    cb(null, d.bugs)
  })
}
