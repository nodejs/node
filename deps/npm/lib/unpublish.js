
module.exports = unpublish

var log = require('npmlog')
var npm = require('./npm.js')
var readJson = require('read-package-json')
var path = require('path')
var mapToRegistry = require('./utils/map-to-registry.js')
var npa = require('npm-package-arg')
var getPublishConfig = require('./utils/get-publish-config.js')
var output = require('./utils/output.js')

unpublish.usage = 'npm unpublish [<@scope>/]<pkg>[@<version>]'

unpublish.completion = function (opts, cb) {
  if (opts.conf.argv.remain.length >= 3) return cb()
  npm.commands.whoami([], true, function (er, username) {
    if (er) return cb()

    var un = encodeURIComponent(username)
    if (!un) return cb()
    var byUser = '-/by-user/' + un
    mapToRegistry(byUser, npm.config, function (er, uri, auth) {
      if (er) return cb(er)

      npm.registry.get(uri, { auth: auth }, function (er, pkgs) {
        // do a bit of filtering at this point, so that we don't need
        // to fetch versions for more than one thing, but also don't
        // accidentally a whole project.
        pkgs = pkgs[un]
        if (!pkgs || !pkgs.length) return cb()
        var pp = npa(opts.partialWord).name
        pkgs = pkgs.filter(function (p) {
          return p.indexOf(pp) === 0
        })
        if (pkgs.length > 1) return cb(null, pkgs)
        mapToRegistry(pkgs[0], npm.config, function (er, uri, auth) {
          if (er) return cb(er)

          npm.registry.get(uri, { auth: auth }, function (er, d) {
            if (er) return cb(er)
            var vers = Object.keys(d.versions)
            if (!vers.length) return cb(null, pkgs)
            return cb(null, vers.map(function (v) {
              return pkgs[0] + '@' + v
            }))
          })
        })
      })
    })
  })
}

function unpublish (args, cb) {
  if (args.length > 1) return cb(unpublish.usage)

  var thing = args.length ? npa(args[0]) : {}
  var project = thing.name
  var version = thing.rawSpec

  log.silly('unpublish', 'args[0]', args[0])
  log.silly('unpublish', 'thing', thing)
  if (!version && !npm.config.get('force')) {
    return cb(
      'Refusing to delete entire project.\n' +
      'Run with --force to do this.\n' +
      unpublish.usage
    )
  }

  if (!project || path.resolve(project) === npm.localPrefix) {
    // if there's a package.json in the current folder, then
    // read the package name and version out of that.
    var cwdJson = path.join(npm.localPrefix, 'package.json')
    return readJson(cwdJson, function (er, data) {
      if (er && er.code !== 'ENOENT' && er.code !== 'ENOTDIR') return cb(er)
      if (er) return cb('Usage:\n' + unpublish.usage)
      log.verbose('unpublish', data)
      gotProject(data.name, data.version, data.publishConfig, cb)
    })
  }
  return gotProject(project, version, cb)
}

function gotProject (project, version, publishConfig, cb_) {
  if (typeof cb_ !== 'function') {
    cb_ = publishConfig
    publishConfig = null
  }

  function cb (er) {
    if (er) return cb_(er)
    output('- ' + project + (version ? '@' + version : ''))
    cb_()
  }

  var mappedConfig = getPublishConfig(publishConfig, npm.config, npm.registry)
  var config = mappedConfig.config
  var registry = mappedConfig.client

  // remove from the cache first
  npm.commands.cache(['clean', project, version], function (er) {
    if (er) {
      log.error('unpublish', 'Failed to clean cache')
      return cb(er)
    }

    mapToRegistry(project, config, function (er, uri, auth) {
      if (er) return cb(er)

      var params = {
        version: version,
        auth: auth
      }
      registry.unpublish(uri, params, cb)
    })
  })
}
