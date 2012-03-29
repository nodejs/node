
module.exports = get

var GET = require("./request.js").GET
  , fs = require("graceful-fs")
  , npm = require("../../npm.js")
  , path = require("path")
  , log = require("../log.js")
  , mkdir = require("mkdirp")
  , cacheStat = null
  , chownr = require("chownr")

function get (project, version, timeout, nofollow, staleOk, cb) {
  if (typeof cb !== "function") cb = staleOk, staleOk = false
  if (typeof cb !== "function") cb = nofollow, nofollow = false
  if (typeof cb !== "function") cb = timeout, timeout = -1
  if (typeof cb !== "function") cb = version, version = null
  if (typeof cb !== "function") cb = project, project = null
  if (typeof cb !== "function") {
    throw new Error("No callback provided to registry.get")
  }

  timeout = Math.min(timeout, npm.config.get("cache-max"))
  timeout = Math.max(timeout, npm.config.get("cache-min"))

  if ( process.env.COMP_CWORD !== undefined
    && process.env.COMP_LINE !== undefined
    && process.env.COMP_POINT !== undefined
    ) timeout = Math.max(timeout, 60000)

  var uri = []
  uri.push(project || "")
  if (version) uri.push(version)
  uri = uri.join("/")

  // /-/all is special.
  // It uses timestamp-based caching and partial updates,
  // because it is a monster.
  if (uri === "/-/all") {
    return requestAll(cb)
  }

  var cache = path.join(npm.cache, uri, ".cache.json")
  fs.stat(cache, function (er, stat) {
    if (!er) fs.readFile(cache, function (er, data) {
      try { data = JSON.parse(data) }
      catch (ex) { data = null }
      get_(uri, timeout, cache, stat, data, nofollow, staleOk, cb)
    })
    else get_(uri, timeout, cache, null, null, nofollow, staleOk, cb)
  })
}

function requestAll (cb) {
  var cache = path.join(npm.cache, "/-/all", ".cache.json")

  mkdir(path.join(npm.cache, "-", "all"), function (er) {
    fs.readFile(cache, function (er, data) {
      if (er) return requestAll_(0, {}, cb)
      try {
        data = JSON.parse(data)
      } catch (ex) {
        fs.writeFile(cache, "{}", function (er) {
          if (er) return cb(new Error("Broken cache. "
                                     +"Please run 'npm cache clean' "
                                     +"and try again."))
          return requestAll_(0, {}, cb)
        })
      }
      var t = +data._updated || 0
      requestAll_(t, data, cb)
    })
  })
}

function requestAll_ (c, data, cb) {
  // use the cache and update in the background if it's not too old
  if (Date.now() - c < 60000) {
    cb(null, data)
    cb = function () {}
  }

  var uri = "/-/all/since?stale=update_after&startkey=" + c

  if (c === 0) {
    log.warn("Building the local index for the first time, please be patient")
    uri = "/-/all"
  }

  var cache = path.join(npm.cache, "-/all", ".cache.json")
  GET(uri, function (er, updates, _, res) {
    if (er) return cb(er, data)
    var headers = res.headers
      , updated = Date.parse(headers.date)
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
      log.verbose("not expired, no request", "registry.get " +uri)
      delete data._etag
      return cb(null, data, JSON.stringify(data), {statusCode:304})
    }
    if (staleOk) {
      log.verbose("staleOk, background update", "registry.get " +uri)
      delete data._etag
      process.nextTick(cb.bind( null, null, data, JSON.stringify(data)
                              , {statusCode: 304} ))
      cb = function () {}
    }
  }

  GET(uri, etag, nofollow, function (er, remoteData, raw, response) {
    // if we get an error talking to the registry, but we have it
    // from the cache, then just pretend we got it.
    if (er && cache && data && !data.error) {
      er = null
      response = {statusCode: 304}
    }

    if (response) {
      log.silly([response.statusCode, response.headers], "get cb")
      if (response.statusCode === 304 && etag) {
        remoteData = data
        log.verbose(uri+" from cache", "etag")
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

    saveToCache(cache, data, saved)
  })
}

function saveToCache (cache, data, saved) {
  if (cacheStat) {
    return saveToCache_(cache, data, cacheStat.uid, cacheStat.gid, saved)
  }
  fs.stat(npm.cache, function (er, st) {
    if (er) {
      return fs.stat(process.env.HOME || "", function (er, st) {
        // if this fails, oh well.
        if (er) return saved()
        cacheStat = st
        return saveToCache(cache, data, saved)
      })
    }
    cacheStat = st || { uid: null, gid: null }
    return saveToCache(cache, data, saved)
  })
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
