'use strict'

const BB = require('bluebird')

const cacache = require('cacache')
const createReadStream = require('graceful-fs').createReadStream
const getPublishConfig = require('./utils/get-publish-config.js')
const lifecycle = BB.promisify(require('./utils/lifecycle.js'))
const log = require('npmlog')
const mapToRegistry = require('./utils/map-to-registry.js')
const npa = require('npm-package-arg')
const npm = require('./npm.js')
const output = require('./utils/output.js')
const pack = require('./pack')
const pacote = require('pacote')
const pacoteOpts = require('./config/pacote')
const path = require('path')
const readJson = BB.promisify(require('read-package-json'))
const readUserInfo = require('./utils/read-user-info.js')
const semver = require('semver')
const statAsync = BB.promisify(require('graceful-fs').stat)

publish.usage = 'npm publish [<tarball>|<folder>] [--tag <tag>] [--access <public|restricted>]' +
                "\n\nPublishes '.' if no argument supplied" +
                '\n\nSets tag `latest` if no --tag specified'

publish.completion = function (opts, cb) {
  // publish can complete to a folder with a package.json
  // or a tarball, or a tarball url.
  // for now, not yet implemented.
  return cb()
}

module.exports = publish
function publish (args, isRetry, cb) {
  if (typeof cb !== 'function') {
    cb = isRetry
    isRetry = false
  }
  if (args.length === 0) args = ['.']
  if (args.length !== 1) return cb(publish.usage)

  log.verbose('publish', args)

  const t = npm.config.get('tag').trim()
  if (semver.validRange(t)) {
    return cb(new Error('Tag name must not be a valid SemVer range: ' + t))
  }

  return publish_(args[0])
    .then((tarball) => {
      const silent = log.level === 'silent'
      if (!silent && npm.config.get('json')) {
        output(JSON.stringify(tarball, null, 2))
      } else if (!silent) {
        output(`+ ${tarball.id}`)
      }
    })
    .nodeify(cb)
}

function publish_ (arg) {
  return statAsync(arg).then((stat) => {
    if (stat.isDirectory()) {
      return stat
    } else {
      const err = new Error('not a directory')
      err.code = 'ENOTDIR'
      throw err
    }
  }).then(() => {
    return publishFromDirectory(arg)
  }, (err) => {
    if (err.code !== 'ENOENT' && err.code !== 'ENOTDIR') {
      throw err
    } else {
      return publishFromPackage(arg)
    }
  })
}

function publishFromDirectory (arg) {
  // All this readJson is because any of the given scripts might modify the
  // package.json in question, so we need to refresh after every step.
  let contents
  return pack.prepareDirectory(arg).then(() => {
    return readJson(path.join(arg, 'package.json'))
  }).then((pkg) => {
    return lifecycle(pkg, 'prepublishOnly', arg)
  }).then(() => {
    return readJson(path.join(arg, 'package.json'))
  }).then((pkg) => {
    return cacache.tmp.withTmp(npm.tmp, {tmpPrefix: 'fromDir'}, (tmpDir) => {
      const target = path.join(tmpDir, 'package.tgz')
      return pack.packDirectory(pkg, arg, target, null, true)
        .tap((c) => { contents = c })
        .then((c) => !npm.config.get('json') && pack.logContents(c))
        .then(() => upload(arg, pkg, false, target))
    })
  }).then(() => {
    return readJson(path.join(arg, 'package.json'))
  }).tap((pkg) => {
    return lifecycle(pkg, 'publish', arg)
  }).tap((pkg) => {
    return lifecycle(pkg, 'postpublish', arg)
  })
    .then(() => contents)
}

function publishFromPackage (arg) {
  return cacache.tmp.withTmp(npm.tmp, {tmpPrefix: 'fromPackage'}, (tmp) => {
    const extracted = path.join(tmp, 'package')
    const target = path.join(tmp, 'package.json')
    const opts = pacoteOpts()
    return pacote.tarball.toFile(arg, target, opts)
      .then(() => pacote.extract(arg, extracted, opts))
      .then(() => readJson(path.join(extracted, 'package.json')))
      .then((pkg) => {
        return BB.resolve(pack.getContents(pkg, target))
          .tap((c) => !npm.config.get('json') && pack.logContents(c))
          .tap(() => upload(arg, pkg, false, target))
      })
  })
}

function upload (arg, pkg, isRetry, cached) {
  if (!pkg) {
    return BB.reject(new Error('no package.json file found'))
  }
  if (pkg.private) {
    return BB.reject(new Error(
      'This package has been marked as private\n' +
      "Remove the 'private' field from the package.json to publish it."
    ))
  }
  const mappedConfig = getPublishConfig(
    pkg.publishConfig,
    npm.config,
    npm.registry
  )
  const config = mappedConfig.config
  const registry = mappedConfig.client

  pkg._npmVersion = npm.version
  pkg._nodeVersion = process.versions.node

  delete pkg.modules

  return BB.fromNode((cb) => {
    mapToRegistry(pkg.name, config, (err, registryURI, auth, registryBase) => {
      if (err) { return cb(err) }
      cb(null, [registryURI, auth, registryBase])
    })
  }).spread((registryURI, auth, registryBase) => {
    // we just want the base registry URL in this case
    log.verbose('publish', 'registryBase', registryBase)
    log.silly('publish', 'uploading', cached)

    pkg._npmUser = {
      name: auth.username,
      email: auth.email
    }

    const params = {
      metadata: pkg,
      body: !npm.config.get('dry-run') && createReadStream(cached),
      auth: auth
    }

    // registry-frontdoor cares about the access level, which is only
    // configurable for scoped packages
    if (config.get('access')) {
      if (!npa(pkg.name).scope && config.get('access') === 'restricted') {
        throw new Error("Can't restrict access to unscoped packages.")
      }

      params.access = config.get('access')
    }

    if (npm.config.get('dry-run')) {
      log.verbose('publish', '--dry-run mode enabled. Skipping upload.')
      return BB.resolve()
    }

    log.showProgress('publish:' + pkg._id)
    return BB.fromNode((cb) => {
      registry.publish(registryBase, params, cb)
    }).catch((err) => {
      if (
        err.code === 'EPUBLISHCONFLICT' &&
        npm.config.get('force') &&
        !isRetry
      ) {
        log.warn('publish', 'Forced publish over ' + pkg._id)
        return BB.fromNode((cb) => {
          npm.commands.unpublish([pkg._id], cb)
        }).finally(() => {
          // ignore errors.  Use the force.  Reach out with your feelings.
          return upload(arg, pkg, true, cached).catch(() => {
            // but if it fails again, then report the first error.
            throw err
          })
        })
      } else {
        throw err
      }
    })
  }).catch((err) => {
    if (err.code !== 'EOTP' && !(err.code === 'E401' && /one-time pass/.test(err.message))) throw err
    // we prompt on stdout and read answers from stdin, so they need to be ttys.
    if (!process.stdin.isTTY || !process.stdout.isTTY) throw err
    return readUserInfo.otp().then((otp) => {
      npm.config.set('otp', otp)
      return upload(arg, pkg, isRetry, cached)
    })
  })
}
