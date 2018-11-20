var url = require("url")

module.exports = function normalize (u) {
  var parsed = url.parse(u, true)

  // git is so tricky!
  // if the path is like ssh://foo:22/some/path then it works, but
  // it needs the ssh://
  // If the path is like ssh://foo:some/path then it works, but
  // only if you remove the ssh://
  if (parsed.protocol) {
    parsed.protocol = parsed.protocol.replace(/^git\+/, "")

    // ssh paths that are scp-style urls don't need the ssh://
    parsed.pathname = parsed.pathname.replace(/^\/?:/, "/")
  }

  // figure out what we should check out.
  var checkout = parsed.hash && parsed.hash.substr(1) || "master"
  parsed.hash = ""

  u = url.format(parsed)
  return {
    url : u,
    branch : checkout
  }
}
