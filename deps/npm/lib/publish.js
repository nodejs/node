
module.exports = publish

var npm = require('./npm.js')
var log = require('npmlog')
var path = require('path')
var readJson = require('read-package-json')
var lifecycle = require('./utils/lifecycle.js')
var chain = require('slide').chain
var mapToRegistry = require('./utils/map-to-registry.js')
var cachedPackageRoot = require('./cache/cached-package-root.js')
var createReadStream = require('graceful-fs').createReadStream
var npa = require('npm-package-arg')
var semver = require('semver')
var getPublishConfig = require('./utils/get-publish-config.js')
var output = require('./utils/output.js')

publish.usage = 'npm publish [<tarball>|<folder>] [--tag <tag>] [--access <public|restricted>]' +
                "\n\nPublishes '.' if no argument supplied" +
                '\n\nSets tag `latest` if no --tag specified'

publish.completion = function (opts, cb) {
  // publish can complete to a folder with a package.json
  // or a tarball, or a tarball url.
  // for now, not yet implemented.
  return cb()
}

function publish (args, isRetry, cb) {
  if (typeof cb !== 'function') {
    cb = isRetry
    isRetry = false
  }
  if (args.length === 0) args = ['.']
  if (args.length !== 1) return cb(publish.usage)

  log.verbose('publish', args)

  var t = npm.config.get('tag').trim()
  if (semver.validRange(t)) {
    var er = new Error('Tag name must not be a valid SemVer range: ' + t)
    return cb(er)
  }

  var arg = args[0]
  // if it's a local folder, then run the prepublish there, first.
  readJson(path.resolve(arg, 'package.json'), function (er, data) {
    if (er && er.code !== 'ENOENT' && er.code !== 'ENOTDIR') return cb(er)

    if (data) {
      if (!data.name) return cb(new Error('No name provided'))
      if (!data.version) return cb(new Error('No version provided'))
    }

    // if readJson errors, the argument might be a tarball or package URL
    if (er) {
      npm.commands.cache.add(arg, null, null, false, function (er, data) {
        if (er) return cb(er)
        log.silly('publish', data)
        var cached = path.resolve(cachedPackageRoot(data), 'package') + '.tgz'
        // *publish* lifecycle scripts aren't run when publishing a built artifact
        // go to the next step directly
        publish_(arg, data, isRetry, cached, cb)
      })
    } else {
      var dir = arg
      npm.commands.cache.add(dir, null, null, false, function (er, data) {
        if (er) return cb(er)
        log.silly('publish', data)
        var cached = path.resolve(cachedPackageRoot(data), 'package') + '.tgz'
        // `prepublish` and `prepare` are run by cache.add
        chain(
          [
            [lifecycle, data, 'prepublishOnly', dir],
            [publish_, dir, data, isRetry, cached],
            [lifecycle, data, 'publish', dir],
            [lifecycle, data, 'postpublish', dir]
          ],
          cb
        )
      })
    }
  })
}

function publish_ (arg, data, isRetry, cached, cb) {
  if (!data) return cb(new Error('no package.json file found'))

  var mappedConfig = getPublishConfig(
    data.publishConfig,
    npm.config,
    npm.registry
  )
  var config = mappedConfig.config
  var registry = mappedConfig.client

  data._npmVersion = npm.version
  data._nodeVersion = process.versions.node

  delete data.modules
  if (data.private) {
    return cb(new Error(
      'This package has been marked as private\n' +
      "Remove the 'private' field from the package.json to publish it."
    ))
  }

  mapToRegistry(data.name, config, function (er, registryURI, auth, registryBase) {
    if (er) return cb(er)

    // we just want the base registry URL in this case
    log.verbose('publish', 'registryBase', registryBase)
    log.silly('publish', 'uploading', cached)

    data._npmUser = {
      name: auth.username,
      email: auth.email
    }

    var params = {
      metadata: data,
      body: createReadStream(cached),
      auth: auth
    }

    // registry-frontdoor cares about the access level, which is only
    // configurable for scoped packages
    if (config.get('access')) {
      if (!npa(data.name).scope && config.get('access') === 'restricted') {
        return cb(new Error("Can't restrict access to unscoped packages."))
      }

      params.access = config.get('access')
    }

    log.showProgress('publish:' + data._id)
    registry.publish(registryBase, params, function (er) {
      if (er && er.code === 'EPUBLISHCONFLICT' &&
          npm.config.get('force') && !isRetry) {
        log.warn('publish', 'Forced publish over ' + data._id)
        return npm.commands.unpublish([data._id], function (er) {
          // ignore errors.  Use the force.  Reach out with your feelings.
          // but if it fails again, then report the first error.
          publish([arg], er || true, cb)
        })
      }
      // report the unpublish error if this was a retry and unpublish failed
      if (er && isRetry && isRetry !== true) return cb(isRetry)
      if (er) return cb(er)
      output('+ ' + data._id)
      cb()
    })
  })
}
