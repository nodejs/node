module.exports = add

var assert = require("assert")
var url = require("url")

var npa = require("npm-package-arg")

function add (uri, params, cb) {
  assert(typeof uri === "string", "must pass registry URI to distTags.add")
  assert(
    params && typeof params === "object",
    "must pass params to distTags.add"
  )
  assert(typeof cb === "function", "muss pass callback to distTags.add")

  assert(
    typeof params.package === "string",
    "must pass package name to distTags.add"
  )
  assert(
    typeof params.distTag === "string",
    "must pass package distTag name to distTags.add"
  )
  assert(
    typeof params.version === "string",
    "must pass version to be mapped to distTag to distTags.add"
  )
  assert(
    params.auth && typeof params.auth === "object",
    "must pass auth to distTags.add"
  )

  var p = npa(params.package)
  var package = p.scope ? params.package.replace("/", "%2f") : params.package
  var rest = "-/package/"+package+"/dist-tags/"+params.distTag

  var options = {
    method : "PUT",
    body : JSON.stringify(params.version),
    auth : params.auth
  }
  this.request(url.resolve(uri, rest), options, cb)
}
