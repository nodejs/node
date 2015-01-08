module.exports = updateIndex

var fs = require("graceful-fs")
  , assert = require("assert")
  , path = require("path")
  , mkdir = require("mkdirp")
  , chownr = require("chownr")
  , url = require("url")
  , npm = require("../npm.js")
  , log = require("npmlog")
  , cacheFile = require("npm-cache-filename")
  , getCacheStat = require("./get-stat.js")

/* /-/all is special.
 * It uses timestamp-based caching and partial updates,
 * because it is a monster.
 */
function updateIndex (uri, params, cb) {
  assert(typeof uri === "string", "must pass registry URI to updateIndex")
  assert(params && typeof params === "object", "must pass params to updateIndex")
  assert(typeof cb === "function", "must pass callback to updateIndex")

  var parsed = url.parse(uri)
  assert(
    parsed.protocol === "http:" || parsed.protocol === "https:",
    "must have a URL that starts with http: or https:"
  )

  var cacheBase = cacheFile(npm.config.get("cache"))(uri)
  var cachePath = path.join(cacheBase, ".cache.json")
  log.info("updateIndex", cachePath)

  getCacheStat(function (er, st) {
    if (er) return cb(er)

    mkdir(cacheBase, function (er, made) {
      if (er) return cb(er)

      fs.readFile(cachePath, function (er, data) {
        if (er) return updateIndex_(uri, params, 0, {}, cachePath, cb)

        try {
          data = JSON.parse(data)
        }
        catch (ex) {
          fs.writeFile(cachePath, "{}", function (er) {
            if (er) return cb(new Error("Broken cache."))

            return updateIndex_(uri, params, 0, {}, cachePath, cb)
          })
        }
        var t = +data._updated || 0
        chownr(made || cachePath, st.uid, st.gid, function (er) {
          if (er) return cb(er)

          updateIndex_(uri, params, t, data, cachePath, cb)
        })
      })
    })
  })
}

function updateIndex_ (uri, params, t, data, cachePath, cb) {
  // use the cache and update in the background if it's not too old
  if (Date.now() - t < 60000) {
    cb(null, data)
    cb = function () {}
  }

  var full
  if (t === 0) {
    log.warn("", "Building the local index for the first time, please be patient")
    full = url.resolve(uri, "/-/all")
  }
  else {
    full = url.resolve(uri, "/-/all/since?stale=update_after&startkey=" + t)
  }

  npm.registry.request(full, params, function (er, updates, _, res) {
    if (er) return cb(er, data)

    var headers = res.headers
    var updated = updates._updated || Date.parse(headers.date)

    Object.keys(updates).forEach(function (p) { data[p] = updates[p] })

    data._updated = updated
    getCacheStat(function (er, st) {
      if (er) return cb(er)

      fs.writeFile(cachePath, JSON.stringify(data), function (er) {
        delete data._updated
        if (er) return cb(er)
        chownr(cachePath, st.uid, st.gid, function (er) {
          cb(er, data)
        })
      })
    })
  })
}
