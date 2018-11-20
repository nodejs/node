var assert = require("assert")

var toNerfDart = require("./nerf-dart.js")

module.exports = clearCredentialsByURI

function clearCredentialsByURI (uri) {
  assert(uri && typeof uri === "string", "registry URL is required")

  var nerfed = toNerfDart(uri)

  this.del(nerfed + ":_authToken", "user")
  this.del(nerfed + ":_password",  "user")
  this.del(nerfed + ":username",   "user")
  this.del(nerfed + ":email",      "user")
}
