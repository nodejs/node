'use strict'

// npm pack <pkg>
// Packs the specified package into a .tgz file, which can then
// be installed.

const BB = require('bluebird')

const cache = require('./cache')
const cacache = require('cacache')
const deprCheck = require('./utils/depr-check')
const fpm = BB.promisify(require('./fetch-package-metadata'))
const fs = require('graceful-fs')
const install = require('./install')
const lifecycle = BB.promisify(require('./utils/lifecycle'))
const move = require('move-concurrently')
const npm = require('./npm')
const output = require('./utils/output')
const pacoteOpts = require('./config/pacote')
const path = require('path')
const pathIsInside = require('path-is-inside')
const pipe = BB.promisify(require('mississippi').pipe)
const prepublishWarning = require('./utils/warn-deprecated')('prepublish-on-install')
const pinflight = require('promise-inflight')
const readJson = BB.promisify(require('read-package-json'))
const tarPack = BB.promisify(require('./utils/tar').pack)
const writeStreamAtomic = require('fs-write-stream-atomic')

pack.usage = 'npm pack [[<@scope>/]<pkg>...]'

// if it can be installed, it can be packed.
pack.completion = install.completion

module.exports = pack
function pack (args, silent, cb) {
  const cwd = process.cwd()
  if (typeof cb !== 'function') {
    cb = silent
    silent = false
  }

  if (args.length === 0) args = ['.']

  BB.all(
    args.map((arg) => pack_(arg, cwd))
  ).then((files) => {
    if (!silent) {
      output(files.map((f) => path.relative(cwd, f)).join('\n'))
    }
    cb(null, files)
  }, cb)
}

// add to cache, then cp to the cwd
function pack_ (pkg, dir) {
  return fpm(pkg, dir).then((mani) => {
    let name = mani.name[0] === '@'
    // scoped packages get special treatment
    ? mani.name.substr(1).replace(/\//g, '-')
    : mani.name
    const target = `${name}-${mani.version}.tgz`
    return pinflight(target, () => {
      if (mani._requested.type === 'directory') {
        return prepareDirectory(mani._resolved).then(() => {
          return packDirectory(mani, mani._resolved, target)
        })
      } else {
        return cache.add(pkg).then((info) => {
          return pipe(
            cacache.get.stream.byDigest(pacoteOpts().cache, info.integrity || mani._integrity),
            writeStreamAtomic(target)
          )
        }).then(() => target)
      }
    })
  })
}

module.exports.prepareDirectory = prepareDirectory
function prepareDirectory (dir) {
  return readJson(path.join(dir, 'package.json')).then((pkg) => {
    if (!pkg.name) {
      throw new Error('package.json requires a "name" field')
    }
    if (!pkg.version) {
      throw new Error('package.json requires a valid "version" field')
    }
    if (!pathIsInside(dir, npm.tmp)) {
      if (pkg.scripts && pkg.scripts.prepublish) {
        prepublishWarning([
          'As of npm@5, `prepublish` scripts are deprecated.',
          'Use `prepare` for build steps and `prepublishOnly` for upload-only',
          'See the deprecation note in `npm help scripts` for more information'
        ])
      }
      if (npm.config.get('ignore-prepublish')) {
        return lifecycle(pkg, 'prepare', dir).then(() => pkg)
      } else {
        return lifecycle(pkg, 'prepublish', dir).then(() => {
          return lifecycle(pkg, 'prepare', dir)
        }).then(() => pkg)
      }
    }
    return pkg
  })
}

module.exports.packDirectory = packDirectory
function packDirectory (mani, dir, target) {
  deprCheck(mani)
  return cacache.tmp.withTmp(npm.tmp, {tmpPrefix: 'packing'}, (tmp) => {
    const tmpTarget = path.join(tmp, path.basename(target))
    return tarPack(tmpTarget, dir, mani).then(() => {
      return move(tmpTarget, target, {Promise: BB, fs})
    }).then(() => target)
  })
}
