var assert = require("assert")
  , url = require("url")
  , zlib = require("zlib")
  , Stream = require("stream").Stream

var rm = require("rimraf")
  , request = require("request")
  , once = require("once")

module.exports = regRequest

// npm: means
// 1. https
// 2. send authorization
// 3. content-type is 'application/json' -- metadata
function regRequest (method, uri, options, cb_) {
  assert(uri, "must pass resource to load")
  assert(cb_, "must pass callback")

  options = options || {}

  var parsed = url.parse(uri)
  var where = parsed.pathname
  var what = options.body
  var follow = (typeof options.follow === "boolean" ? options.follow : true)
  this.log.verbose("request", "on initialization, where is", where)

  if (parsed.search) {
    where = where + parsed.search
    parsed.search = ""
  }
  parsed.pathname = "/"
  this.log.verbose("request", "after pass 1, where is", where)

  // Since there are multiple places where an error could occur,
  // don't let the cb be called more than once.
  var cb = once(cb_)

  if (where.match(/^\/?favicon.ico/)) {
    return cb(new Error("favicon.ico isn't a package, it's a picture."))
  }

  var adduserChange = /^\/?-\/user\/org\.couchdb\.user:([^\/]+)\/-rev/
    , isUserChange = where.match(adduserChange)
    , adduserNew = /^\/?-\/user\/org\.couchdb\.user:([^\/]+)$/
    , isNewUser = where.match(adduserNew)
    , registry = url.format(parsed)
    , alwaysAuth = this.conf.getCredentialsByURI(registry).alwaysAuth
    , isDelete = method === "DELETE"
    , isWrite = what || isDelete

  if (isUserChange && !isWrite) {
    return cb(new Error("trying to change user document without writing(?!)"))
  }

  // resolve to a full url on the registry
  if (!where.match(/^https?:\/\//)) {
    this.log.verbose("request", "url raw", where)

    var q = where.split("?")
    where = q.shift()
    q = q.join("?")

    if (where.charAt(0) !== "/") where = "/" + where
    where = "." + where.split("/").map(function (p) {
      p = p.trim()
      if (p.match(/^org.couchdb.user/)) {
        return p.replace(/\//g, encodeURIComponent("/"))
      }
      return p
    }).join("/")
    if (q) where += "?" + q

    this.log.verbose("request", "resolving registry", [registry, where])
    where = url.resolve(registry, where)
    this.log.verbose("request", "after pass 2, where is", where)
  }

  var authed
  // new users can *not* use auth, because they don't *have* auth yet
  if (isNewUser) {
    this.log.verbose("request", "new user, so can't send auth")
    authed = false
  }
  else if (alwaysAuth) {
    this.log.verbose("request", "always-auth set; sending authorization")
    authed = true
  }
  else if (isWrite) {
    this.log.verbose("request", "sending authorization for write operation")
    authed = true
  }
  else {
    // most of the time we don't want to auth
    this.log.verbose("request", "no auth needed")
    authed = false
  }

  var self = this
  this.attempt(function (operation) {
    makeRequest.call(self, method, where, what, options.etag, follow, authed
                     , function (er, parsed, raw, response) {
      if (!er || (er.message && er.message.match(/^SSL Error/))) {
        if (er)
          er.code = "ESSL"
        return cb(er, parsed, raw, response)
      }

      // Only retry on 408, 5xx or no `response`.
      var statusCode = response && response.statusCode

      var timeout = statusCode === 408
      var serverError = statusCode >= 500
      var statusRetry = !statusCode || timeout || serverError
      if (er && statusRetry && operation.retry(er)) {
        self.log.info("retry", "will retry, error on last attempt: " + er)
        return undefined
      }
      if (response) {
        self.log.verbose("headers", response.headers)
        if (response.headers["npm-notice"]) {
          self.log.warn("notice", response.headers["npm-notice"])
        }
      }
      cb.apply(null, arguments)
    })
  })
}

function makeRequest (method, where, what, etag, follow, authed, cb_) {
  var cb = once(cb_)

  var parsed = url.parse(where)
  var headers = {}

  // metadata should be compressed
  headers["accept-encoding"] = "gzip"

  var er = this.authify(authed, parsed, headers)
  if (er) return cb_(er)

  var opts = this.initialize(
    parsed,
    method,
    "application/json",
    headers
  )

  opts.followRedirect = follow
  opts.encoding = null // tell request let body be Buffer instance

  if (etag) {
    this.log.verbose("etag", etag)
    headers[method === "GET" ? "if-none-match" : "if-match"] = etag
  }

  // figure out wth "what" is
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

  this.log.http("request", method, parsed.href || "/")

  var done = requestDone.call(this, method, where, cb)
  var req = request(opts, decodeResponseBody(done))

  req.on("error", cb)
  req.on("socket", function (s) {
    s.on("error", cb)
  })

  if (what && (what instanceof Stream)) {
    what.pipe(req)
  }
}

function decodeResponseBody(cb) {
  return function (er, response, data) {
    if (er) return cb(er, response, data)

    // don't ever re-use connections that had server errors.
    // those sockets connect to the Bad Place!
    if (response.socket && response.statusCode > 500) {
      response.socket.destroy()
    }

    if (response.headers["content-encoding"] !== "gzip") return cb(er, response, data)

    zlib.gunzip(data, function (er, buf) {
      if (er) return cb(er, response, data)

      cb(null, response, buf)
    })
  }
}

// cb(er, parsed, raw, response)
function requestDone (method, where, cb) {
  return function (er, response, data) {
    if (er) return cb(er)

    var urlObj = url.parse(where)
    if (urlObj.auth)
      urlObj.auth = "***"
    this.log.http(response.statusCode, url.format(urlObj))

    var parsed

    if (Buffer.isBuffer(data)) {
      data = data.toString()
    }

    if (data && typeof data === "string" && response.statusCode !== 304) {
      try {
        parsed = JSON.parse(data)
      } catch (ex) {
        ex.message += "\n" + data
        this.log.verbose("bad json", data)
        this.log.error("registry", "error parsing json")
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

    er = null
    if (parsed && response.headers.etag) {
      parsed._etag = response.headers.etag
    }

    if (parsed && parsed.error && response.statusCode >= 400) {
      var w = url.parse(where).pathname.substr(1)
      var name
      if (!w.match(/^-/)) {
        w = w.split("/")
        name = w[w.indexOf("_rewrite") + 1]
      }

      if (name && parsed.error === "not_found") {
        er = new Error("404 Not Found: " + name)
      } else {
        er = new Error(
          parsed.error + " " + (parsed.reason || "") + ": " + w)
      }
      if (name) er.pkgid = name
      er.statusCode = response.statusCode
      er.code = "E" + er.statusCode

    } else if (method !== "HEAD" && method !== "GET") {
      // invalidate cache
      // This is irrelevant for commands that do etag caching, but
      // ls and view also have a timed cache, so this keeps the user
      // from thinking that it didn't work when it did.
      // Note that failure is an acceptable option here, since the
      // only result will be a stale cache for some helper commands.
      rm(this.cacheFile(where), function() {})
    }
    return cb(er, parsed, data, response)
  }.bind(this)
}
