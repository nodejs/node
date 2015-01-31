module.exports = tag

var assert = require("assert")

function tag (uri, params, cb) {
  assert(typeof uri === "string", "must pass registry URI to tag")
  assert(params && typeof params === "object", "must pass params to tag")
  assert(typeof cb === "function", "must pass callback to tag")

  assert(typeof params.version === "string", "must pass version to tag")
  assert(typeof params.tag === "string", "must pass tag name to tag")
  assert(
    params.auth && typeof params.auth === "object",
    "must pass auth to tag"
  )

  var options = {
    method : "PUT",
    body : JSON.stringify(params.version),
    auth : params.auth
  }
  this.request(uri+"/"+params.tag, options, cb)
}
