module.exports = CachingRegistryClient

var path = require("path")
  , fs = require("graceful-fs")
  , url = require("url")
  , assert = require("assert")
  , inherits = require("util").inherits

var RegistryClient = require("npm-registry-client")
  , npm = require("../npm.js")
  , log = require("npmlog")
  , getCacheStat = require("./get-stat.js")
  , cacheFile = require("npm-cache-filename")
  , mkdirp = require("mkdirp")
  , rimraf = require("rimraf")
  , chownr = require("chownr")
  , writeFile = require("write-file-atomic")

function CachingRegistryClient (config) {
  RegistryClient.call(this, adaptConfig(config))

  this._mapToCache = cacheFile(config.get("cache"))

  // swizzle in our custom cache invalidation logic
  this._request = this.request
  this.request  = this._invalidatingRequest
  this.get      = get
}
inherits(CachingRegistryClient, RegistryClient)

CachingRegistryClient.prototype._invalidatingRequest = function (uri, params, cb) {
  var client = this
  this._request.call(this, uri, params, function () {
    var args = arguments

    var method = params.method
    if (method !== "HEAD" && method !== "GET") {
      var invalidated = client._mapToCache(uri)
      // invalidate cache
      //
      // This is irrelevant for commands that do etag / last-modified caching,
      // but ls and view also have a timed cache, so this keeps the user from
      // thinking that it didn't work when it did.
      // Note that failure is an acceptable option here, since the only
      // result will be a stale cache for some helper commands.
      log.verbose("request", "invalidating", invalidated, "on", method)
      return rimraf(invalidated, function () {
        cb.apply(undefined, args)
      })
    }

    cb.apply(undefined, args)
  })
}

function get (uri, params, cb) {
  assert(typeof uri === "string", "must pass registry URI to get")
  assert(params && typeof params === "object", "must pass params to get")
  assert(typeof cb === "function", "must pass callback to get")

  var parsed = url.parse(uri)
  assert(
    parsed.protocol === "http:" || parsed.protocol === "https:",
    "must have a URL that starts with http: or https:"
  )

  var cacheBase = cacheFile(npm.config.get("cache"))(uri)
  var cachePath = path.join(cacheBase, ".cache.json")

  // If the GET is part of a write operation (PUT or DELETE), then
  // skip past the cache entirely, but still save the results.
  if (uri.match(/\?write=true$/)) {
    log.verbose("get", "GET as part of write; not caching result")
    return get_.call(this, uri, cachePath, params, cb)
  }

  var client = this
  fs.stat(cachePath, function (er, stat) {
    if (!er) {
      fs.readFile(cachePath, function (er, data) {
        try {
          data = JSON.parse(data)
        }
        catch (ex) {
          data = null
        }

        params.stat = stat
        params.data = data

        get_.call(client, uri, cachePath, params, cb)
      })
    }
    else {
      get_.call(client, uri, cachePath, params, cb)
    }
  })
}

function get_ (uri, cachePath, params, cb) {
  var staleOk = params.staleOk === undefined ? false : params.staleOk
    , timeout = params.timeout === undefined ? -1 : params.timeout
    , data    = params.data
    , stat    = params.stat
    , etag
    , lastModified

  timeout = Math.min(timeout, npm.config.get("cache-max") || 0)
  timeout = Math.max(timeout, npm.config.get("cache-min") || -Infinity)
  if (process.env.COMP_CWORD !== undefined &&
      process.env.COMP_LINE  !== undefined &&
      process.env.COMP_POINT !== undefined) {
    timeout = Math.max(timeout, 60000)
  }

  if (data) {
    if (data._etag) etag = data._etag
    if (data._lastModified) lastModified = data._lastModified

    if (stat && timeout && timeout > 0) {
      if ((Date.now() - stat.mtime.getTime())/1000 < timeout) {
        log.verbose("get", uri, "not expired, no request")
        delete data._etag
        delete data._lastModified
        return cb(null, data, JSON.stringify(data), { statusCode : 304 })
      }

      if (staleOk) {
        log.verbose("get", uri, "staleOk, background update")
        delete data._etag
        delete data._lastModified
        process.nextTick(
          cb.bind(null, null, data, JSON.stringify(data), { statusCode : 304 } )
        )
        cb = function () {}
      }
    }
  }

  var options = {
    etag         : etag,
    lastModified : lastModified,
    follow       : params.follow,
    auth         : params.auth
  }
  this.request(uri, options, function (er, remoteData, raw, response) {
    // if we get an error talking to the registry, but we have it
    // from the cache, then just pretend we got it.
    if (er && cachePath && data && !data.error) {
      er = null
      response = { statusCode: 304 }
    }

    if (response) {
      log.silly("get", "cb", [response.statusCode, response.headers])
      if (response.statusCode === 304 && (etag || lastModified)) {
        remoteData = data
        log.verbose(etag ? "etag" : "lastModified", uri+" from cache")
      }
    }

    data = remoteData
    if (!data) er = er || new Error("failed to fetch from registry: " + uri)

    if (er) return cb(er, data, raw, response)

    saveToCache(cachePath, data, saved)

    // just give the write the old college try.  if it fails, whatever.
    function saved () {
      delete data._etag
      delete data._lastModified
      cb(er, data, raw, response)
    }

    function saveToCache (cachePath, data, saved) {
      log.verbose("get", "saving", data.name, "to", cachePath)
      getCacheStat(function (er, st) {
        mkdirp(path.dirname(cachePath), function (er, made) {
          if (er) return saved()

          writeFile(cachePath, JSON.stringify(data), function (er) {
            if (er) return saved()

            chownr(made || cachePath, st.uid, st.gid, saved)
          })
        })
      })
    }
  })
}

function adaptConfig (config) {
  return {
    proxy : {
      http         : config.get("proxy"),
      https        : config.get("https-proxy"),
      localAddress : config.get("local-address")
    },
    ssl : {
      certificate : config.get("cert"),
      key         : config.get("key"),
      ca          : config.get("ca"),
      strict      : config.get("strict-ssl")
    },
    retry : {
      retries    : config.get("fetch-retries"),
      factor     : config.get("fetch-retry-factor"),
      minTimeout : config.get("fetch-retry-mintimeout"),
      maxTimeout : config.get("fetch-retry-maxtimeout")
    },
    userAgent  : config.get("user-agent"),
    log        : log,
    defaultTag : config.get("tag"),
    couchToken : config.get("_token"),
    maxSockets : config.get('maxsockets')
  }
}
