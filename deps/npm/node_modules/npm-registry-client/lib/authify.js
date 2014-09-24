var url = require("url")

module.exports = authify

function authify (authed, parsed, headers) {
  var c = this.conf.getCredentialsByURI(url.format(parsed))

  if (c && c.token) {
    this.log.verbose("request", "using bearer token for auth")
    headers.authorization = "Bearer " + c.token

    return null
  }

  if (authed) {
    if (c && c.username && c.password) {
      var username = encodeURIComponent(c.username)
      var password = encodeURIComponent(c.password)
      parsed.auth = username + ":" + password
    }
    else {
      return new Error(
        "This request requires auth credentials. Run `npm login` and repeat the request."
      )
    }
  }
}
