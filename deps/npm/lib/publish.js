'use strict'

const BB = require('bluebird')

const cacache = require('cacache')
const figgyPudding = require('figgy-pudding')
const libpub = require('libnpm/publish')
const libunpub = require('libnpm/unpublish')
const lifecycle = BB.promisify(require('./utils/lifecycle.js'))
const log = require('npmlog')
const npa = require('libnpm/parse-arg')
const npmConfig = require('./config/figgy-config.js')
const output = require('./utils/output.js')
const otplease = require('./utils/otplease.js')
const pack = require('./pack')
const { tarball, extract } = require('libnpm')
const path = require('path')
const readFileAsync = BB.promisify(require('graceful-fs').readFile)
const readJson = BB.promisify(require('read-package-json'))
const semver = require('semver')
const statAsync = BB.promisify(require('graceful-fs').stat)

publish.usage = 'npm publish [<tarball>|<folder>] [--tag <tag>] [--access <public|restricted>] [--dry-run]' +
                "\n\nPublishes '.' if no argument supplied" +
                '\n\nSets tag `latest` if no --tag specified'

publish.completion = function (opts, cb) {
  // publish can complete to a folder with a package.json
  // or a tarball, or a tarball url.
  // for now, not yet implemented.
  return cb()
}

const PublishConfig = figgyPudding({
  dryRun: 'dry-run',
  'dry-run': { default: false },
  force: { default: false },
  json: { default: false },
  Promise: { default: () => Promise },
  tag: { default: 'latest' },
  tmp: {}
})

module.exports = publish
function publish (args, isRetry, cb) {
  if (typeof cb !== 'function') {
    cb = isRetry
    isRetry = false
  }
  if (args.length === 0) args = ['.']
  if (args.length !== 1) return cb(publish.usage)

  log.verbose('publish', args)

  const opts = PublishConfig(npmConfig())
  const t = opts.tag.trim()
  if (semver.validRange(t)) {
    return cb(new Error('Tag name must not be a valid SemVer range: ' + t))
  }

  return publish_(args[0], opts)
    .then((tarball) => {
      const silent = log.level === 'silent'
      if (!silent && opts.json) {
        output(JSON.stringify(tarball, null, 2))
      } else if (!silent) {
        output(`+ ${tarball.id}`)
      }
    })
    .nodeify(cb)
}

function publish_ (arg, opts) {
  return statAsync(arg).then((stat) => {
    if (stat.isDirectory()) {
      return stat
    } else {
      const err = new Error('not a directory')
      err.code = 'ENOTDIR'
      throw err
    }
  }).then(() => {
    return publishFromDirectory(arg, opts)
  }, (err) => {
    if (err.code !== 'ENOENT' && err.code !== 'ENOTDIR') {
      throw err
    } else {
      return publishFromPackage(arg, opts)
    }
  })
}

function publishFromDirectory (arg, opts) {
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
    return cacache.tmp.withTmp(opts.tmp, {tmpPrefix: 'fromDir'}, (tmpDir) => {
      const target = path.join(tmpDir, 'package.tgz')
      return pack.packDirectory(pkg, arg, target, null, true)
        .tap((c) => { contents = c })
        .then((c) => !opts.json && pack.logContents(c))
        .then(() => upload(pkg, false, target, opts))
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

function publishFromPackage (arg, opts) {
  return cacache.tmp.withTmp(opts.tmp, {tmpPrefix: 'fromPackage'}, tmp => {
    const extracted = path.join(tmp, 'package')
    const target = path.join(tmp, 'package.json')
    return tarball.toFile(arg, target, opts)
      .then(() => extract(arg, extracted, opts))
      .then(() => readJson(path.join(extracted, 'package.json')))
      .then((pkg) => {
        return BB.resolve(pack.getContents(pkg, target))
          .tap((c) => !opts.json && pack.logContents(c))
          .tap(() => upload(pkg, false, target, opts))
      })
  })
}

function upload (pkg, isRetry, cached, opts) {
  if (!opts.dryRun) {
    return readFileAsync(cached).then(tarball => {
      return otplease(opts, opts => {
        return libpub(pkg, tarball, opts)
      }).catch(err => {
        if (
          err.code === 'EPUBLISHCONFLICT' &&
          opts.force &&
          !isRetry
        ) {
          log.warn('publish', 'Forced publish over ' + pkg._id)
          return otplease(opts, opts => libunpub(
            npa.resolve(pkg.name, pkg.version), opts
          )).finally(() => {
            // ignore errors.  Use the force.  Reach out with your feelings.
            return otplease(opts, opts => {
              return upload(pkg, true, tarball, opts)
            }).catch(() => {
              // but if it fails again, then report the first error.
              throw err
            })
          })
        } else {
          throw err
        }
      })
    })
  } else {
    return opts.Promise.resolve(true)
  }
}
