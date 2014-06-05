
module.exports = tag

function tag (project, version, tagName, cb) {
  this.request("PUT", project+"/"+tagName, JSON.stringify(version), cb)
}
