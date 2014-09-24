var crypto = require("crypto")

var pkg = require("../package.json")

module.exports = initialize

function initialize (uri, method, accept, headers) {
  if (!this.sessionToken) {
    this.sessionToken = crypto.randomBytes(8).toString("hex")
    this.log.verbose("request id", this.sessionToken)
  }

  var strict = this.conf.get("strict-ssl")
  if (strict === undefined) strict = true

  var p = this.conf.get("proxy")
  var sp = this.conf.get("https-proxy") || p

  var opts = {
    url          : uri,
    method       : method,
    headers      : headers,
    proxy        : uri.protocol === "https:" ? sp : p,
    localAddress : this.conf.get("local-address"),
    strictSSL    : strict,
    cert         : this.conf.get("cert"),
    key          : this.conf.get("key"),
    ca           : this.conf.get("ca")
  }

  headers.version = this.version || pkg.version
  headers.accept = accept

  if (this.refer) headers.referer = this.refer

  headers["npm-session"] = this.sessionToken
  headers["user-agent"] = this.conf.get("user-agent") ||
                          "node/" + process.version

  return opts
}
