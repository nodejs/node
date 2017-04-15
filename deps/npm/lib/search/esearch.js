'use strict'

var npm = require('../npm.js')
var log = require('npmlog')
var mapToRegistry = require('../utils/map-to-registry.js')
var jsonstream = require('JSONStream')
var ms = require('mississippi')
var gunzip = require('../utils/gunzip-maybe')

module.exports = esearch

function esearch (opts) {
  var stream = ms.through.obj()

  mapToRegistry('-/v1/search', npm.config, function (er, uri, auth) {
    if (er) return stream.emit('error', er)
    createResultStream(uri, auth, opts, function (err, resultStream) {
      if (err) return stream.emit('error', err)
      ms.pipeline.obj(resultStream, stream)
    })
  })
  return stream
}

function createResultStream (uri, auth, opts, cb) {
  log.verbose('esearch', 'creating remote entry stream')
  var params = {
    timeout: 600,
    follow: true,
    staleOk: true,
    auth: auth,
    streaming: true
  }
  var q = buildQuery(opts)
  npm.registry.request(uri + '?text=' + encodeURIComponent(q) + '&size=' + opts.limit, params, function (err, res) {
    if (err) return cb(err)
    log.silly('esearch', 'request stream opened, code:', res.statusCode)
    // NOTE - The stream returned by `request` seems to be very persnickety
    //        and this is almost a magic incantation to get it to work.
    //        Modify how `res` is used here at your own risk.
    var entryStream = ms.pipeline.obj(
      res,
      ms.through(function (chunk, enc, cb) {
        cb(null, chunk)
      }),
      gunzip(),
      jsonstream.parse('objects.*.package', function (data) {
        return {
          name: data.name,
          description: data.description,
          maintainers: data.maintainers,
          keywords: data.keywords,
          version: data.version,
          date: data.date ? new Date(data.date) : null
        }
      })
    )
    return cb(null, entryStream)
  })
}

function buildQuery (opts) {
  return opts.include.join(' ')
}
