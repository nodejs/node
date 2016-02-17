module.exports = updateIndex

var fs = require('graceful-fs')
var assert = require('assert')
var path = require('path')
var mkdir = require('mkdirp')
var chownr = require('chownr')
var npm = require('../npm.js')
var log = require('npmlog')
var cacheFile = require('npm-cache-filename')
var getCacheStat = require('./get-stat.js')
var mapToRegistry = require('../utils/map-to-registry.js')
var pulseTillDone = require('../utils/pulse-till-done.js')
var parseJSON = require('../utils/parse-json.js')

/* /-/all is special.
 * It uses timestamp-based caching and partial updates,
 * because it is a monster.
 */
function updateIndex (staleness, cb) {
  assert(typeof cb === 'function', 'must pass callback to updateIndex')

  mapToRegistry('-/all', npm.config, function (er, uri, auth) {
    if (er) return cb(er)

    var params = {
      timeout: staleness,
      follow: true,
      staleOk: true,
      auth: auth
    }
    var cacheBase = cacheFile(npm.config.get('cache'))(uri)
    var cachePath = path.join(cacheBase, '.cache.json')
    log.info('updateIndex', cachePath)

    getCacheStat(function (er, st) {
      if (er) return cb(er)

      mkdir(cacheBase, function (er, made) {
        if (er) return cb(er)

        fs.readFile(cachePath, function (er, data) {
          if (er) {
            log.warn('', 'Building the local index for the first time, please be patient')
            return updateIndex_(uri, params, {}, cachePath, cb)
          }

          chownr(made || cachePath, st.uid, st.gid, function (er) {
            if (er) return cb(er)

            data = parseJSON.noExceptions(data)
            if (!data) {
              fs.writeFile(cachePath, '{}', function (er) {
                if (er) return cb(new Error('Broken cache.'))

                log.warn('', 'Building the local index for the first time, please be patient')
                return updateIndex_(uri, params, {}, cachePath, cb)
              })
            }

            var t = +data._updated || 0
            // use the cache and update in the background if it's not too old
            if (Date.now() - t < 60000) {
              cb(null, data)
              cb = function () {}
            }

            if (t === 0) {
              log.warn('', 'Building the local index for the first time, please be patient')
            } else {
              log.verbose('updateIndex', 'Cached search data present with timestamp', t)
              uri += '/since?stale=update_after&startkey=' + t
            }
            updateIndex_(uri, params, data, cachePath, cb)
          })
        })
      })
    })
  })
}

function updateIndex_ (all, params, data, cachePath, cb) {
  log.silly('update-index', 'fetching', all)
  npm.registry.request(all, params, pulseTillDone('updateIndex', function (er, updates, _, res) {
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
  }))
}
