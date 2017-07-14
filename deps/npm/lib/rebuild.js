
module.exports = rebuild

var readInstalled = require('read-installed')
var semver = require('semver')
var log = require('npmlog')
var npm = require('./npm.js')
var npa = require('npm-package-arg')
var usage = require('./utils/usage')
var output = require('./utils/output.js')

rebuild.usage = usage(
  'rebuild',
  'npm rebuild [[<@scope>/<name>]...]'
)

rebuild.completion = require('./utils/completion/installed-deep.js')

function rebuild (args, cb) {
  var opt = { depth: npm.config.get('depth'), dev: true }
  readInstalled(npm.prefix, opt, function (er, data) {
    log.info('readInstalled', typeof data)
    if (er) return cb(er)
    var set = filter(data, args)
    var folders = Object.keys(set).filter(function (f) {
      return f !== npm.prefix
    })
    if (!folders.length) return cb()
    log.silly('rebuild set', folders)
    cleanBuild(folders, set, cb)
  })
}

function cleanBuild (folders, set, cb) {
  npm.commands.build(folders, function (er) {
    if (er) return cb(er)
    output(folders.map(function (f) {
      return set[f] + ' ' + f
    }).join('\n'))
    cb()
  })
}

function filter (data, args, set, seen) {
  if (!set) set = {}
  if (!seen) seen = new Set()
  if (set.hasOwnProperty(data.path)) return set
  if (seen.has(data)) return set
  seen.add(data)
  var pass
  if (!args.length) pass = true // rebuild everything
  else if (data.name && data._id) {
    for (var i = 0, l = args.length; i < l; i++) {
      var arg = args[i]
      var nv = npa(arg)
      var n = nv.name
      var v = nv.rawSpec
      if (n !== data.name) continue
      if (!semver.satisfies(data.version, v, true)) continue
      pass = true
      break
    }
  }
  if (pass && data._id) {
    log.verbose('rebuild', 'path, id', [data.path, data._id])
    set[data.path] = data._id
  }
  // need to also dive through kids, always.
  // since this isn't an install these won't get auto-built unless
  // they're not dependencies.
  Object.keys(data.dependencies || {}).forEach(function (d) {
    // return
    var dep = data.dependencies[d]
    if (typeof dep === 'string') return
    filter(dep, args, set, seen)
  })
  return set
}
