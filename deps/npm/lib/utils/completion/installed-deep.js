module.exports = installedDeep

var npm = require('../../npm.js')
var readInstalled = require('read-installed')

function installedDeep (opts, cb) {
  var local
  var global
  var depth = npm.config.get('depth')
  var opt = { depth: depth, dev: true }

  if (npm.config.get('global')) {
    local = []
    next()
  } else {
    readInstalled(npm.prefix, opt, function (er, data) {
      local = getNames(data || {})
      next()
    })
  }

  readInstalled(npm.config.get('prefix'), opt, function (er, data) {
    global = getNames(data || {})
    next()
  })

  function getNames_ (d, n) {
    if (d.realName && n) {
      if (n[d.realName]) return n
      n[d.realName] = true
    }
    if (!n) n = {}
    Object.keys(d.dependencies || {}).forEach(function (dep) {
      getNames_(d.dependencies[dep], n)
    })
    return n
  }
  function getNames (d) {
    return Object.keys(getNames_(d))
  }

  function next () {
    if (!local || !global) return
    if (!npm.config.get('global')) {
      global = global.map(function (g) {
        return [g, '-g']
      })
    }
    var names = local.concat(global)
    return cb(null, names)
  }
}
