/**
 * Fetch an HTTP url to a local file.
 **/

var request = require("request")
  , fs = require("graceful-fs")
  , npm = require("../npm.js")
  , url = require("url")
  , log = require("npmlog")
  , path = require("path")
  , mkdir = require("mkdirp")
  , chownr = require("chownr")
  , regHost
  , once = require("once")

module.exports = fetch

function fetch (remote, local, headers, cb) {
  if (typeof cb !== "function") cb = headers, headers = {}
  cb = once(cb)
  log.verbose("fetch", "to=", local)
  mkdir(path.dirname(local), function (er, made) {
    if (er) return cb(er)
    fetch_(remote, local, headers, cb)
  })
}

function fetch_ (remote, local, headers, cb) {
  var fstr = fs.createWriteStream(local, { mode : npm.modes.file })
  var response = null

  fstr.on("error", function (er) {
    cb(er)
    fstr.close()
  })

  var req = makeRequest(remote, fstr, headers)
  req.on("response", function (res) {
    log.http(res.statusCode, remote)
    response = res
    response.resume()
    // Work around bug in node v0.10.0 where the CryptoStream
    // gets stuck and never starts reading again.
    if (process.version === "v0.10.0") {
      response.resume = function (orig) { return function() {
        var ret = orig.apply(response, arguments)
        if (response.socket.encrypted)
          response.socket.encrypted.read(0)
        return ret
      }}(response.resume)
    }
  })

  fstr.on("close", function () {
    var er
    if (response && response.statusCode && response.statusCode >= 400) {
      er = new Error(response.statusCode + " "
                    + require("http").STATUS_CODES[response.statusCode])
    }
    cb(er, response)
  })
}

function makeRequest (remote, fstr, headers) {
  remote = url.parse(remote)
  log.http("GET", remote.href)
  regHost = regHost || url.parse(npm.config.get("registry")).host

  if (remote.host === regHost && npm.config.get("always-auth")) {
    remote.auth = new Buffer( npm.config.get("_auth")
                            , "base64" ).toString("utf8")
    if (!remote.auth) return fstr.emit("error", new Error(
      "Auth required and none provided. Please run 'npm adduser'"))
  }

  var proxy
  if (remote.protocol !== "https:" || !(proxy = npm.config.get("https-proxy"))) {
    proxy = npm.config.get("proxy")
  }

  var opts = { url: remote
             , proxy: proxy
             , strictSSL: npm.config.get("strict-ssl")
             , rejectUnauthorized: npm.config.get("strict-ssl")
             , ca: remote.host === regHost ? npm.config.get("ca") : undefined
             , headers: { "user-agent": npm.config.get("user-agent") }}
  var req = request(opts)
  req.on("error", function (er) {
    fstr.emit("error", er)
  })
  req.pipe(fstr)
  return req
}
