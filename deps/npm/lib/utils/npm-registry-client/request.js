module.exports = regRequest

regRequest.GET = GET
regRequest.PUT = PUT
regRequest.reg = reg
regRequest.upload = upload

var npm = require("../../npm.js")
  , url = require("url")
  , log = require("../log.js")
  , fs = require("graceful-fs")
  , rm = require("rimraf")
  , asyncMap = require("slide").asyncMap
  , warnedAuth = false
  , newloctimeout = 0
  , stream = require("stream")
  , Stream = stream.Stream
  , request = require("request")

function regRequest (method, where, what, etag, nofollow, cb_) {
  if (typeof cb_ !== "function") cb_ = nofollow, nofollow = false
  if (typeof cb_ !== "function") cb_ = etag, etag = null
  if (typeof cb_ !== "function") cb_ = what, what = null

  // Since there are multiple places where an error could occur,
  // don't let the cb be called more than once.
  var errState = null
  function cb (er) {
    if (errState) return
    if (er) errState = er
    cb_.apply(null, arguments)
  }

  if (where.match(/^\/?favicon.ico/)) {
    return cb(new Error("favicon.ico isn't a package, it's a picture."))
  }

  var registry = reg()
  if (registry instanceof Error) return cb(registry)

  var adduserChange = /^\/?-\/user\/org\.couchdb\.user:([^\/]+)\/-rev/
    , adduserNew = /^\/?-\/user\/org\.couchdb\.user:([^\/]+)/
    , authRequired = (what || npm.config.get("always-auth"))
                      && !where.match(adduserNew)
                   || where.match(adduserChange)
                   || method === "DELETE"

  // resolve to a full url on the registry
  if (!where.match(/^https?:\/\//)) {
    log.verbose(where, "raw, before any munging")

    var q = where.split("?")
    where = q.shift()
    q = q.join("?")

    if (where.charAt(0) !== "/") where = "/" + where
    where = "." + where.split("/").map(function (p) {
      p = p.trim()
      if (p.match(/^org.couchdb.user/)) {
        return p.replace(/\//g, encodeURIComponent("/"))
      }
      return encodeURIComponent(p)
    }).join("/")
    if (q) where += "?" + q
    log.verbose([registry, where], "url resolving")
    where = url.resolve(registry, where)
    log.verbose(where, "url resolved")
  }

  var remote = url.parse(where)
    , auth = authRequired && npm.config.get("_auth")

  if (authRequired && !auth) {
    return cb(new Error(
      "Cannot insert data into the registry without authorization\n"
      + "See: npm-adduser(1)"))
  }

  if (auth) remote.auth = new Buffer(auth, "base64").toString("utf8")

  makeRequest(method, remote, where, what, etag, nofollow, cb)
}

function makeRequest (method, remote, where, what, etag, nofollow, cb) {
  var opts = { url: remote
             , method: method
             , ca: npm.config.get("ca")
             , strictSSL: npm.config.get("strict-ssl") }
    , headers = opts.headers = {}
  if (etag) {
    log.verbose(etag, "etag")
    headers[method === "GET" ? "if-none-match" : "if-match"] = etag
  }

  headers.accept = "application/json"

  headers["user-agent"] = npm.config.get("user-agent")

  opts.proxy = npm.config.get( remote.protocol === "https:"
                             ? "https-proxy" : "proxy" )

  // figure out wth 'what' is
  if (what) {
    if (Buffer.isBuffer(what) || typeof what === "string") {
      opts.body = what
      headers["content-type"] = "application/json"
      headers["content-length"] = Buffer.byteLength(what)
    } else if (what instanceof Stream) {
      headers["content-type"] = "application/octet-stream"
      if (what.size) headers["content-length"] = what.size
    } else {
      delete what._etag
      opts.json = what
    }
  }

  if (nofollow) {
    opts.followRedirect = false
  }

  log.http(remote.href || "/", method)

  var req = request(opts, requestDone(method, where, cb))
  var r = npm.config.get("registry")
  if (!r) {
    return new Error("Must define registry URL before accessing registry.")
  }

  req.on("error", cb)

  if (what && (what instanceof Stream)) {
    what.pipe(req)
  }
}

// cb(er, parsed, raw, response)
function requestDone (method, where, cb) { return function (er, response, data) {
  if (er) return cb(er)

  log.http(response.statusCode + " " + url.parse(where).href)

  var parsed

  if (Buffer.isBuffer(data)) {
    data = data.toString()
  }

  if (data && typeof data === "string" && response.statusCode !== 304) {
    try {
      parsed = JSON.parse(data)
    } catch (ex) {
      ex.message += "\n" + data
      log.verbose(data, "bad json")
      log.error("error parsing json", "registry")
      return cb(ex, null, data, response)
    }
  } else if (data) {
    parsed = data
    data = JSON.stringify(parsed)
  }

  // expect data with any error codes
  if (!data && response.statusCode >= 400) {
    return cb( response.statusCode + " "
             + require("http").STATUS_CODES[response.statusCode]
             , null, data, response )
  }

  var er = null
  if (parsed && response.headers.etag) {
    parsed._etag = response.headers.etag
  }

  if (parsed && parsed.error && response.statusCode >= 400) {
    var w = url.parse(where).pathname.substr(1)
    if (!w.match(/^-/) && parsed.error === "not_found") {
      w = w.split("/")
      name = w[w.indexOf("_rewrite") + 1]
      er = new Error("404 Not Found: "+name)
      er.errno = npm.E404
      er.pkgid = name
    } else {
      er = new Error(
        parsed.error + " " + (parsed.reason || "") + ": " + w)
    }
  } else if (method !== "HEAD" && method !== "GET") {
    // invalidate cache
    // This is irrelevant for commands that do etag caching, but
    // ls and view also have a timed cache, so this keeps the user
    // from thinking that it didn't work when it did.
    // Note that failure is an acceptable option here, since the
    // only result will be a stale cache for some helper commands.
    var path = require("path")
      , p = url.parse(where).pathname.split("/")
      , _ = "/"
      , caches = p.map(function (part) {
          return _ = path.join(_, part)
        }).map(function (cache) {
          return path.join(npm.cache, cache, ".cache.json")
        })

    // if the method is DELETE, then also remove the thing itself.
    // Note that the search index is probably invalid.  Whatever.
    // That's what you get for deleting stuff.  Don't do that.
    if (method === "DELETE") {
      p = p.slice(0, p.indexOf("-rev"))
      caches.push(path.join(npm.cache, p.join("/")))
    }

    asyncMap(caches, rm, function () {})
  }
  return cb(er, parsed, data, response)
}}

function GET (where, etag, nofollow, cb) {
  regRequest("GET", where, null, etag, nofollow, cb)
}

function PUT (where, what, etag, nofollow, cb) {
  regRequest("PUT", where, what, etag, nofollow, cb)
}

function upload (where, filename, etag, nofollow, cb) {
  if (typeof nofollow === "function") cb = nofollow, nofollow = false
  if (typeof etag === "function") cb = etag, etag = null

  fs.stat(filename, function (er, stat) {
    if (er) return cb(er)
    var s = fs.createReadStream(filename)
    s.size = stat.size
    s.on("error", cb)

    PUT(where, s, etag, nofollow, cb)
  })
}

function reg () {
  var r = npm.config.get("registry")
  if (!r) {
    return new Error("Must define registry URL before accessing registry.")
  }
  if (r.substr(-1) !== "/") r += "/"
  npm.config.set("registry", r)
  return r
}
