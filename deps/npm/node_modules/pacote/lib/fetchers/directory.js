'use strict'

const BB = require('bluebird')

const Fetcher = require('../fetch')
const glob = BB.promisify(require('glob'))
const packDir = require('../util/pack-dir')
const path = require('path')
const pipe = BB.promisify(require('mississippi').pipe)
const through = require('mississippi').through

const readFileAsync = BB.promisify(require('fs').readFile)

const fetchDirectory = module.exports = Object.create(null)

Fetcher.impl(fetchDirectory, {
  // `directory` manifests come from the actual manifest/lockfile data.
  manifest (spec, opts) {
    const pkgPath = path.join(spec.fetchSpec, 'package.json')
    const srPath = path.join(spec.fetchSpec, 'npm-shrinkwrap.json')
    return BB.join(
      readFileAsync(pkgPath).then(JSON.parse).catch({code: 'ENOENT'}, err => {
        err.code = 'ENOPACKAGEJSON'
        throw err
      }),
      readFileAsync(srPath).then(JSON.parse).catch({code: 'ENOENT'}, () => null),
      (pkg, sr) => {
        pkg._shrinkwrap = sr
        pkg._hasShrinkwrap = !!sr
        pkg._resolved = spec.fetchSpec
        pkg._integrity = false // Don't auto-calculate integrity
        return pkg
      }
    ).then(pkg => {
      if (!pkg.bin && pkg.directories && pkg.directories.bin) {
        const dirBin = pkg.directories.bin
        return glob(path.join(spec.fetchSpec, dirBin, '/**'), {nodir: true}).then(matches => {
          matches.forEach(filePath => {
            const relative = path.relative(spec.fetchSpec, filePath)
            if (relative && relative[0] !== '.') {
              if (!pkg.bin) { pkg.bin = {} }
              pkg.bin[path.basename(relative)] = relative
            }
          })
        }).then(() => pkg)
      } else {
        return pkg
      }
    })
  },

  // As of npm@5, the npm installer doesn't pack + install directories: it just
  // creates symlinks. This code is here because `npm pack` still needs the
  // ability to create a tarball from a local directory.
  tarball (spec, opts) {
    const stream = through()
    this.manifest(spec, opts).then(mani => {
      return pipe(this.fromManifest(mani, spec, opts), stream)
    }).catch(err => stream.emit('error', err))
    return stream
  },

  // `directory` tarballs are generated in a very similar way to git tarballs.
  fromManifest (manifest, spec, opts) {
    const stream = through()
    packDir(manifest, manifest._resolved, manifest._resolved, stream, opts).catch(err => {
      stream.emit('error', err)
    })
    return stream
  }
})
