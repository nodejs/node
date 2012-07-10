
module.exports = tag

function tag (project, version, tag, cb) {
  this.request("PUT", project+"/"+tag, JSON.stringify(version), cb)
}
