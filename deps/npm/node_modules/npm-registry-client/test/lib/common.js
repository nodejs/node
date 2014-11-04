var resolve = require("path").resolve

var server = require("./server.js")
var RC = require("../../")
var toNerfDart = require("../../lib/util/nerf-dart.js")

var REGISTRY = "http://localhost:" + server.port

module.exports = {
  port : server.port,
  registry : REGISTRY,
  freshClient : function freshClient(config) {
    config = config || {}
    config.cache = resolve(__dirname, "../fixtures/cache")
    config.registry = REGISTRY
    var container = {
      get: function (k) { return config[k] },
      set: function (k, v) { config[k] = v },
      del: function (k) { delete config[k] },
      getCredentialsByURI: function(uri) {
        var nerfed = toNerfDart(uri)
        var c = {scope : nerfed}

        if (this.get(nerfed + ":_authToken")) {
          c.token = this.get(nerfed + ":_authToken")
          // the bearer token is enough, don't confuse things
          return c
        }

        if (this.get(nerfed + ":_password")) {
          c.password = new Buffer(this.get(nerfed + ":_password"), "base64").toString("utf8")
        }

        if (this.get(nerfed + ":username")) {
          c.username = this.get(nerfed + ":username")
        }

        if (this.get(nerfed + ":email")) {
          c.email = this.get(nerfed + ":email")
        }

        if (this.get(nerfed + ":always-auth") !== undefined) {
          c.alwaysAuth = this.get(nerfed + ":always-auth")
        }

        if (c.username && c.password) {
          c.auth = new Buffer(c.username + ":" + c.password).toString("base64")
        }

        return c
      },
      setCredentialsByURI: function (uri, c) {
        var nerfed = toNerfDart(uri)

        if (c.token) {
          this.set(nerfed + ":_authToken", c.token, "user")
          this.del(nerfed + ":_password", "user")
          this.del(nerfed + ":username", "user")
          this.del(nerfed + ":email", "user")
        }
        else if (c.username || c.password || c.email) {
          this.del(nerfed + ":_authToken", "user")

          var encoded = new Buffer(c.password, "utf8").toString("base64")
          this.set(nerfed + ":_password", encoded, "user")
          this.set(nerfed + ":username", c.username, "user")
          this.set(nerfed + ":email", c.email, "user")
        }
        else {
          throw new Error("No credentials to set.")
        }
      }
    }

    var client = new RC(container)
    server.log = client.log
    client.log.level = "silent"

    return client
  }
}
