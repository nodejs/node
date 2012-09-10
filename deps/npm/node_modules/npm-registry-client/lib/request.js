module.exports = regRequest

var url = require("url")
  , fs = require("graceful-fs")
  , rm = require("rimraf")
  , asyncMap = require("slide").asyncMap
  , Stream = require("stream").Stream
  , request = require("request")
  , retry = require("retry")

function regRequest (method, where, what, etag, nofollow, cb_) {
  if (typeof cb_ !== "function") cb_ = nofollow, nofollow = false
  if (typeof cb_ !== "function") cb_ = etag, etag = null
  if (typeof cb_ !== "function") cb_ = what, what = null

  var registry = this.conf.get('registry')
  if (!registry) return cb(new Error(
    "No registry url provided: " + method + " " + where))

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
  , isUpload = what || this.conf.get('always-auth')
  , isDel = method === "DELETE"
  , authRequired = isUpload && !nu || uc || isDel

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

  var remote = url.parse(where)
  , auth = this.conf.get('_auth')

  if (authRequired && !this.conf.get('always-auth')) {
    var couch = this.couchLogin
    , token = couch && (this.conf.get('_token') || couch.token)
    , validToken = token && couch.valid(token)

    if (!validToken) token = null
    else this.conf.set('_token', token)

    if (couch && !token) {
      // login to get a valid token
      var a = { name: this.conf.get('username'),
                password: this.conf.get('_password') }
      var args = arguments
      return this.couchLogin.login(a, function (er, cr, data) {
        if (er || !couch.valid(couch.token)) {
          er = er || new Error('login error')
          return cb(er, cr, data)
        }
        this.conf.set('_token', this.couchLogin.token)
        return regRequest.call(this,
                               method, where, what,
                               etag, nofollow, cb_)
      }.bind(this))
    }
  }

  // now we either have a valid token, or an auth.

  if (authRequired && !auth && !token) {
    return cb(new Error(
      "Cannot insert data into the registry without auth"))
  }

  if (auth && !token) {
    remote.auth = new Buffer(auth, "base64").toString("utf8")
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
    makeRequest.call(self, method, remote, where, what, etag, nofollow, token
                     , function (er, parsed, raw, response) {
      if (!er || er.message.match(/^SSL Error/)) {
        if (er)
          er.code = 'ESSL'
        return cb(er, parsed, raw, response)
      }

      // Only retry on 408, 5xx or no `response`.
      var statusCode = response && response.statusCode
      var reauth = statusCode === 401
      var timeout = statusCode === 408
      var serverError = statusCode >= 500
      var statusRetry = !statusCode || timeout || serverError
      if (reauth && this.conf.get('_auth') && this.conf.get('_token')) {
        this.conf.del('_token')
        this.couchLogin.token = null
        return regRequest.call(this, method, where, what, etag, nofollow, cb_)
      }
      if (er && statusRetry && operation.retry(er)) {
        self.log.info("retry", "will retry, error on last attempt: " + er)
        return
      }
      cb.apply(null, arguments)
    }.bind(this))
  }.bind(this))
}

function makeRequest (method, remote, where, what, etag, nofollow, tok, cb_) {
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
             , ca: this.conf.get('ca')
             , strictSSL: strict }
    , headers = opts.headers = {}
  if (etag) {
    this.log.verbose("etag", etag)
    headers[method === "GET" ? "if-none-match" : "if-match"] = etag
  }

  if (tok) {
    headers.cookie = 'AuthSession=' + tok.AuthSession
  }

  headers.accept = "application/json"

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
  var req = request(opts, done)

  req.on("error", cb)
  req.on("socket", function (s) {
    s.on("error", cb)
  })

  if (what && (what instanceof Stream)) {
    what.pipe(req)
  }
}

// cb(er, parsed, raw, response)
function requestDone (method, where, cb) {
  return function (er, response, data) {
    if (er) return cb(er)

    this.log.http(response.statusCode, url.parse(where).href)

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
      var path = require("path")
        , p = url.parse(where).pathname.split("/")
        , _ = "/"
        , caches = p.map(function (part) {
            return _ = path.join(_, part)
          }).map(function (cache) {
            return path.join(this.conf.get('cache'), cache, ".cache.json")
          }, this)

      // if the method is DELETE, then also remove the thing itself.
      // Note that the search index is probably invalid.  Whatever.
      // That's what you get for deleting stuff.  Don't do that.
      if (method === "DELETE") {
        p = p.slice(0, p.indexOf("-rev"))
        caches.push(path.join(this.conf.get('cache'), p.join("/")))
      }

      asyncMap(caches, rm, function () {})
    }
    return cb(er, parsed, data, response)
  }.bind(this)
}
