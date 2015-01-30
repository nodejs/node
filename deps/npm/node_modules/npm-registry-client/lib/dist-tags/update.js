module.exports = update

var assert = require("assert")
var url = require("url")

var npa = require("npm-package-arg")

function update (uri, params, cb) {
  assert(typeof uri === "string", "must pass registry URI to distTags.update")
  assert(
    params && typeof params === "object",
    "must pass params to distTags.update"
  )
  assert(typeof cb === "function", "muss pass callback to distTags.update")

  assert(
    typeof params.package === "string",
    "must pass package name to distTags.update"
  )
  assert(
    params.distTags && typeof params.distTags === "object",
    "must pass distTags map to distTags.update"
  )
  assert(
    params.auth && typeof params.auth === "object",
    "must pass auth to distTags.update"
  )

  var p = npa(params.package)
  var package = p.scope ? params.package.replace("/", "%2f") : params.package
  var rest = "-/package/"+package+"/dist-tags"

  var options = {
    method : "POST",
    body : JSON.stringify(params.distTags),
    auth : params.auth
  }
  this.request(url.resolve(uri, rest), options, cb)
}
