
module.exports = get

var fs = require("graceful-fs")
  , path = require("path")
  , mkdir = require("mkdirp")
  , chownr = require("chownr")

function get (uri, timeout, nofollow, staleOk, cb) {
  if (typeof cb !== "function") cb = staleOk, staleOk = false
  if (typeof cb !== "function") cb = nofollow, nofollow = false
  if (typeof cb !== "function") cb = timeout, timeout = -1
  if (typeof cb !== "function") cb = version, version = null

  timeout = Math.min(timeout, this.conf.get('cache-max') || 0)
  timeout = Math.max(timeout, this.conf.get('cache-min') || -Infinity)

  if (!this.conf.get('registry')) timeout = Infinity

  if ( process.env.COMP_CWORD !== undefined
    && process.env.COMP_LINE !== undefined
    && process.env.COMP_POINT !== undefined
    ) timeout = Math.max(timeout, 60000)

  // /-/all is special.
  // It uses timestamp-based caching and partial updates,
  // because it is a monster.
  if (uri === "/-/all") {
    return requestAll.call(this, cb)
  }

  var cacheUri = uri
  // on windows ":" is not an allowed character in a foldername
  cacheUri = cacheUri.replace(/:/g, '_').replace(/\?write=true$/, '')
  var cache = path.join(this.conf.get('cache'), cacheUri, ".cache.json")

  // If the GET is part of a write operation (PUT or DELETE), then
  // skip past the cache entirely, but still save the results.
  if (uri.match(/\?write=true$/))
    return get_.call(this, uri, timeout, cache, null, null, nofollow, staleOk, cb)


  fs.stat(cache, function (er, stat) {
    if (!er) fs.readFile(cache, function (er, data) {
      try { data = JSON.parse(data) }
      catch (ex) { data = null }
      get_.call(this, uri, timeout, cache, stat, data, nofollow, staleOk, cb)
    }.bind(this))
    else get_.call(this, uri, timeout, cache, null, null, nofollow, staleOk, cb)
  }.bind(this))
}

function requestAll (cb) {
  var cache = path.join(this.conf.get('cache'), "/-/all", ".cache.json")

  mkdir(path.join(this.conf.get('cache'), "-", "all"), function (er) {
    fs.readFile(cache, function (er, data) {
      if (er) return requestAll_.call(this, 0, {}, cb)
      try {
        data = JSON.parse(data)
      } catch (ex) {
        fs.writeFile(cache, "{}", function (er) {
          if (er) return cb(new Error("Broken cache."))
          return requestAll_.call(this, 0, {}, cb)
        }.bind(this))
      }
      var t = +data._updated || 0
      requestAll_.call(this, t, data, cb)
    }.bind(this))
  }.bind(this))
}

function requestAll_ (c, data, cb) {
  // use the cache and update in the background if it's not too old
  if (Date.now() - c < 60000) {
    cb(null, data)
    cb = function () {}
  }

  var uri = "/-/all/since?stale=update_after&startkey=" + c

  if (c === 0) {
    this.log.warn("", "Building the local index for the first time, please be patient")
    uri = "/-/all"
  }

  var cache = path.join(this.conf.get('cache'), "-/all", ".cache.json")
  this.request('GET', uri, function (er, updates, _, res) {
    if (er) return cb(er, data)
    var headers = res.headers
      , updated = updates._updated || Date.parse(headers.date)
    Object.keys(updates).forEach(function (p) {
      data[p] = updates[p]
    })
    data._updated = updated
    fs.writeFile( cache, JSON.stringify(data)
                , function (er) {
      delete data._updated
      return cb(er, data)
    })
  })
}

function get_ (uri, timeout, cache, stat, data, nofollow, staleOk, cb) {
  var etag
  if (data && data._etag) etag = data._etag
  if (timeout && timeout > 0 && stat && data) {
    if ((Date.now() - stat.mtime.getTime())/1000 < timeout) {
      this.log.verbose("registry.get", uri, "not expired, no request")
      delete data._etag
      return cb(null, data, JSON.stringify(data), {statusCode:304})
    }
    if (staleOk) {
      this.log.verbose("registry.get", uri, "staleOk, background update")
      delete data._etag
      process.nextTick(cb.bind( null, null, data, JSON.stringify(data)
                              , {statusCode: 304} ))
      cb = function () {}
    }
  }

  this.request('GET', uri, null, etag, nofollow, function (er, remoteData, raw, response) {
    // if we get an error talking to the registry, but we have it
    // from the cache, then just pretend we got it.
    if (er && cache && data && !data.error) {
      er = null
      response = {statusCode: 304}
    }

    if (response) {
      this.log.silly("registry.get", "cb", [response.statusCode, response.headers])
      if (response.statusCode === 304 && etag) {
        remoteData = data
        this.log.verbose("etag", uri+" from cache")
      }
    }

    data = remoteData
    if (!data) {
      er = er || new Error("failed to fetch from registry: " + uri)
    }

    if (er) return cb(er, data, raw, response)

    // just give the write the old college try.  if it fails, whatever.
    function saved () {
      delete data._etag
      cb(er, data, raw, response)
    }

    saveToCache.call(this, cache, data, saved)
  }.bind(this))
}

function saveToCache (cache, data, saved) {
  if (this._cacheStat) {
    var cs = this._cacheStat
    return saveToCache_.call(this, cache, data, cs.uid, cs.gid, saved)
  }
  fs.stat(this.conf.get('cache'), function (er, st) {
    if (er) {
      return fs.stat(process.env.HOME || "", function (er, st) {
        // if this fails, oh well.
        if (er) return saved()
        this._cacheStat = st
        return saveToCache.call(this, cache, data, saved)
      }.bind(this))
    }
    this._cacheStat = st || { uid: null, gid: null }
    return saveToCache.call(this, cache, data, saved)
  }.bind(this))
}

function saveToCache_ (cache, data, uid, gid, saved) {
  mkdir(path.dirname(cache), function (er, made) {
    if (er) return saved()
    fs.writeFile(cache, JSON.stringify(data), function (er) {
      if (er || uid === null || gid === null) {
        return saved()
      }
      chownr(made || cache, uid, gid, saved)
    })
  })
}
