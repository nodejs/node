module.exports = regRequest

var url = require("url")
  , zlib = require("zlib")
  , assert = require("assert")
  , rm = require("rimraf")
  , Stream = require("stream").Stream
  , request = require("request")
  , retry = require("retry")
  , crypto = require("crypto")
  , pkg = require("../package.json")


// npm: means
// 1. https
// 2. send authorization
// 3. content-type is 'application/json' -- metadata
function regRequest (method, uri, options, cb_) {
  assert(uri, "must pass resource to load")
  assert(cb_, "must pass callback")

  options = options || {}
  var nofollow = (typeof options.follow === 'boolean' ? !options.follow : false)
  var etag = options.etag
  var what = options.body

  var parsed = url.parse(uri)

  var authThis = false
  if (parsed.protocol === "npm") {
    parsed.protocol = "https"
    authThis = true
  }

  var where = parsed.pathname
  if (parsed.search) {
    where = where + parsed.search
    parsed.search = ""
  }
  parsed.pathname = "/"
  this.log.verbose("request", "where is", where)

  var registry = url.format(parsed)
  this.log.verbose("request", "registry", registry)

  if (!this.sessionToken) {
    this.sessionToken = crypto.randomBytes(8).toString("hex")
    this.log.verbose("request id", this.sessionToken)
  }

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

  var adduserChange = /^\/?-\/user\/org\.couchdb\.user:([^\/]+)\/-rev/
  , adduserNew = /^\/?-\/user\/org\.couchdb\.user:([^\/]+)/
  , nu = where.match(adduserNew)
  , uc = where.match(adduserChange)
  , alwaysAuth = this.conf.get('always-auth')
  , isDel = method === "DELETE"
  , isWrite = what || isDel
  , authRequired = (authThis || alwaysAuth || isWrite) && !nu || uc || isDel

  // resolve to a full url on the registry
  if (!where.match(/^https?:\/\//)) {
    this.log.verbose("url raw", where)

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
    this.log.verbose("url resolving", [registry, where])
    where = url.resolve(registry, where)
    this.log.verbose("url resolved", where)
  }
  this.log.verbose("request", "where is", where)

  var remote = url.parse(where)
  , auth = this.conf.get('_auth')

  if (authRequired && !auth) {
    var un = this.conf.get('username')
    var pw = this.conf.get('_password')
    if (un && pw)
      auth = new Buffer(un + ':' + pw).toString('base64')
  }

  if (authRequired && !auth) {
    return cb(new Error(
      "This request requires auth credentials. Run `npm login` and repeat the request."))
  }

  if (auth && authRequired) {
    // Escape any weird characters that might be in the auth string
    // TODO(isaacs) Clean up this awful back and forth mess.
    var remoteAuth = new Buffer(auth, "base64").toString("utf8")
    remoteAuth = encodeURIComponent(remoteAuth).replace(/%3A/, ":")
    remote.auth = remoteAuth
  }

  // Tuned to spread 3 attempts over about a minute.
  // See formula at <https://github.com/tim-kos/node-retry>.
  var operation = retry.operation({
    retries: this.conf.get('fetch-retries') || 2,
    factor: this.conf.get('fetch-retry-factor'),
    minTimeout: this.conf.get('fetch-retry-mintimeout') || 10000,
    maxTimeout: this.conf.get('fetch-retry-maxtimeout') || 60000
  })

  var self = this
  operation.attempt(function (currentAttempt) {
    self.log.info("trying", "registry request attempt " + currentAttempt
        + " at " + (new Date()).toLocaleTimeString())
    makeRequest.call(self, method, remote, where, what, etag, nofollow
                     , function (er, parsed, raw, response) {
      if (!er || (er.message && er.message.match(/^SSL Error/))) {
        if (er)
          er.code = 'ESSL'
        return cb(er, parsed, raw, response)
      }

      // Only retry on 408, 5xx or no `response`.
      var statusCode = response && response.statusCode

      var timeout = statusCode === 408
      var serverError = statusCode >= 500
      var statusRetry = !statusCode || timeout || serverError
      if (er && statusRetry && operation.retry(er)) {
        self.log.info("retry", "will retry, error on last attempt: " + er)
        return
      }
      if (response) {
        this.log.verbose("headers", response.headers)
        if (response.headers["npm-notice"]) {
          this.log.warn("notice", response.headers["npm-notice"])
        }
      }
      cb.apply(null, arguments)
    }.bind(this))
  }.bind(this))
}

function makeRequest (method, remote, where, what, etag, nofollow, cb_) {
  var cbCalled = false
  function cb () {
    if (cbCalled) return
    cbCalled = true
    cb_.apply(null, arguments)
  }

  var strict = this.conf.get('strict-ssl')
  if (strict === undefined) strict = true
  var opts = { url: remote
             , method: method
             , encoding: null // tell request let body be Buffer instance
             , ca: this.conf.get('ca')
             , localAddress: this.conf.get('local-address')
             , cert: this.conf.get('cert')
             , key: this.conf.get('key')
             , strictSSL: strict }
    , headers = opts.headers = {}
  if (etag) {
    this.log.verbose("etag", etag)
    headers[method === "GET" ? "if-none-match" : "if-match"] = etag
  }

  headers['npm-session'] = this.sessionToken
  headers.version = this.version || pkg.version

  if (this.refer) {
    headers.referer = this.refer
  }

  headers.accept = "application/json"
  headers['accept-encoding'] = 'gzip'

  headers["user-agent"] = this.conf.get('user-agent') ||
                          'node/' + process.version

  var p = this.conf.get('proxy')
  var sp = this.conf.get('https-proxy') || p
  opts.proxy = remote.protocol === "https:" ? sp : p

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

  this.log.http(method, remote.href || "/")

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

    if (response.headers['content-encoding'] !== 'gzip') return cb(er, response, data)

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
      urlObj.auth = '***'
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
      if (!w.match(/^-/) && parsed.error === "not_found") {
        w = w.split("/")
        name = w[w.indexOf("_rewrite") + 1]
        er = new Error("404 Not Found: "+name)
        er.code = "E404"
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
      rm(this.cacheFile(where), function() {})
    }
    return cb(er, parsed, data, response)
  }.bind(this)
}
