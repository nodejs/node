module.exports = fetch

var assert = require("assert")
var url = require("url")

var npa = require("npm-package-arg")

function fetch (uri, params, cb) {
  assert(typeof uri === "string", "must pass registry URI to distTags.fetch")
  assert(
    params && typeof params === "object",
    "must pass params to distTags.fetch"
  )
  assert(typeof cb === "function", "muss pass callback to distTags.fetch")

  assert(
    typeof params.package === "string",
    "must pass package name to distTags.fetch"
  )
  assert(
    params.auth && typeof params.auth === "object",
    "must pass auth to distTags.fetch"
  )

  var p = npa(params.package)
  var package = p.scope ? params.package.replace("/", "%2f") : params.package
  var rest = "-/package/"+package+"/dist-tags"

  var options = {
    method : "GET",
    auth : params.auth
  }
  this.request(url.resolve(uri, rest), options, function (er, data) {
    if (data && typeof data === "object") delete data._etag
    cb(er, data)
  })
}
