module.exports = star

var npm = require('./npm.js')
var log = require('npmlog')
var asyncMap = require('slide').asyncMap
var mapToRegistry = require('./utils/map-to-registry.js')
var usage = require('./utils/usage')

star.usage = usage(
  'star',
  'npm star [<pkg>...]\n' +
  'npm unstar [<pkg>...]'
)

star.completion = function (opts, cb) {
  // FIXME: there used to be registry completion here, but it stopped making
  // sense somewhere around 50,000 packages on the registry
  cb()
}

function star (args, cb) {
  if (!args.length) return cb(star.usage)
  var s = npm.config.get('unicode') ? '\u2605 ' : '(*)'
  var u = npm.config.get('unicode') ? '\u2606 ' : '( )'
  var using = !(npm.command.match(/^un/))
  if (!using) s = u
  asyncMap(args, function (pkg, cb) {
    mapToRegistry(pkg, npm.config, function (er, uri, auth) {
      if (er) return cb(er)

      var params = {
        starred: using,
        auth: auth
      }
      npm.registry.star(uri, params, function (er, data, raw, req) {
        if (!er) {
          console.log(s + ' ' + pkg)
          log.verbose('star', data)
        }
        cb(er, data, raw, req)
      })
    })
  }, cb)
}
