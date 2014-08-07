module.exports = tag

function tag (uri, version, tagName, cb) {
  this.request("PUT", uri+"/"+tagName, { body : JSON.stringify(version) }, cb)
}
