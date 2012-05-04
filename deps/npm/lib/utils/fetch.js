/**
 * Fetch an HTTP url to a local file.
 **/

var request = require("request")
  , fs = require("graceful-fs")
  , npm = require("../npm.js")
  , url = require("url")
  , log = require("./log.js")
  , path = require("path")
  , mkdir = require("./mkdir-p.js")
  , regHost
  , getAgent = require("./get-agent.js")

module.exports = fetch

function fetch (remote, local, headers, cb) {
  if (typeof cb !== "function") cb = headers, headers = {}
  log.verbose(local, "fetch to")
  mkdir(path.dirname(local), function (er) {
    if (er) return cb(er)
    fetch_(remote, local, headers, cb)
  })
}

function fetch_ (remote, local, headers, cb) {
  var fstr = fs.createWriteStream(local, { mode : npm.modes.file })
  fstr.on("error", function (er) {
    fs.close(fstr.fd, function () {})
    if (fstr._ERROR) return
    cb(fstr._ERROR = er)
  })
  fstr.on("open", function () {
    makeRequest(remote, fstr, headers)
  })
  fstr.on("close", function () {
    if (fstr._ERROR) return
    cb()
  })
}

function makeRequest (remote, fstr, headers) {
  remote = url.parse(remote)
  log.http(remote.href, "GET")
  regHost = regHost || url.parse(npm.config.get("registry")).host

  if (remote.host === regHost && npm.config.get("always-auth")) {
    remote.auth = new Buffer( npm.config.get("_auth")
                            , "base64" ).toString("utf8")
    if (!remote.auth) return fstr.emit("error", new Error(
      "Auth required and none provided. Please run 'npm adduser'"))
  }

  var proxy = npm.config.get( remote.protocol === "https:"
                            ? "https-proxy"
                            : "proxy")

  request({ url: remote
          , proxy: proxy
          , agent: getAgent(remote)
          , strictSSL: npm.config.get("strict-ssl")
          , onResponse: onResponse }).pipe(fstr)
  function onResponse (er, res) {
    if (er) return fstr.emit("error", er)
    log.http(res.statusCode + " " + remote.href)
  }
}
