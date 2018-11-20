'use strict'

const BB = require('bluebird')

const cacache = require('cacache')
const cacheKey = require('./util/cache-key')
const fetchFromManifest = require('./fetch').fromManifest
const finished = require('./util/finished')
const minimatch = require('minimatch')
const normalize = require('normalize-package-data')
const optCheck = require('./util/opt-check')
const path = require('path')
const pipe = BB.promisify(require('mississippi').pipe)
const ssri = require('ssri')
const tar = require('tar')

// `finalizeManifest` takes as input the various kinds of manifests that
// manifest handlers ('lib/fetchers/*.js#manifest()') return, and makes sure
// they are:
//
// * filled out with any required data that the handler couldn't fill in
// * formatted consistently
// * cached so we don't have to repeat this work more than necessary
//
// The biggest thing this package might do is do a full tarball extraction in
// order to find missing bits of metadata required by the npm installer. For
// example, it will fill in `_shrinkwrap`, `_integrity`, and other details that
// the plain manifest handlers would require a tarball to fill out. If a
// handler returns everything necessary, this process is skipped.
//
// If we get to the tarball phase, the corresponding tarball handler for the
// requested type will be invoked and the entire tarball will be read from the
// stream.
//
module.exports = finalizeManifest
function finalizeManifest (pkg, spec, opts) {
  const key = finalKey(pkg, spec)
  opts = optCheck(opts)

  const cachedManifest = (opts.cache && key && !opts.preferOnline && !opts.fullMetadata)
    ? cacache.get.info(opts.cache, key, opts)
    : BB.resolve(null)

  return cachedManifest.then(cached => {
    if (cached && cached.metadata.manifest) {
      return new Manifest(cached.metadata.manifest)
    } else {
      return tarballedProps(pkg, spec, opts).then(props => {
        return pkg && pkg.name
          ? new Manifest(pkg, props, opts.fullMetadata)
          : new Manifest(props, null, opts.fullMetadata)
      }).then(manifest => {
        const cacheKey = key || finalKey(manifest, spec)
        if (!opts.cache || !cacheKey) {
          return manifest
        } else {
          opts.metadata = {
            id: manifest._id,
            manifest,
            type: 'finalized-manifest'
          }
          return cacache.put(
            opts.cache, cacheKey, '.', opts
          ).then(() => manifest)
        }
      })
    }
  })
}

module.exports.Manifest = Manifest
function Manifest (pkg, fromTarball, fullMetadata) {
  fromTarball = fromTarball || {}
  if (fullMetadata) {
    Object.assign(this, pkg)
  }
  this.name = pkg.name
  this.version = pkg.version
  this.engines = pkg.engines || fromTarball.engines
  this.cpu = pkg.cpu || fromTarball.cpu
  this.os = pkg.os || fromTarball.os
  this.dependencies = pkg.dependencies || {}
  this.optionalDependencies = pkg.optionalDependencies || {}
  this.devDependencies = pkg.devDependencies || {}
  const bundled = (
    pkg.bundledDependencies ||
    pkg.bundleDependencies ||
    false
  )
  this.bundleDependencies = bundled
  this.peerDependencies = pkg.peerDependencies || {}
  this.deprecated = pkg.deprecated || false

  // These depend entirely on each handler
  this._resolved = pkg._resolved

  // Not all handlers (or registries) provide these out of the box,
  // and if they don't, we need to extract and read the tarball ourselves.
  // These are details required by the installer.
  this._integrity = pkg._integrity || fromTarball._integrity || null
  this._shasum = pkg._shasum || fromTarball._shasum || null
  this._shrinkwrap = pkg._shrinkwrap || fromTarball._shrinkwrap || null
  this.bin = pkg.bin || fromTarball.bin || null

  if (this.bin && Array.isArray(this.bin)) {
    // Code yanked from read-package-json.
    const m = (pkg.directories && pkg.directories.bin) || '.'
    this.bin = this.bin.reduce((acc, mf) => {
      if (mf && mf.charAt(0) !== '.') {
        const f = path.basename(mf)
        acc[f] = path.join(m, mf)
      }
      return acc
    }, {})
  }

  this._id = null

  // TODO - freezing and inextensibility pending npm changes. See test suite.
  // Object.preventExtensions(this)
  normalize(this)

  // I don't want this why did you give it to me. Go away. 🔥🔥🔥🔥
  delete this.readme

  // Object.freeze(this)
}

// Some things aren't filled in by standard manifest fetching.
// If this function needs to do its work, it will grab the
// package tarball, extract it, and take whatever it needs
// from the stream.
function tarballedProps (pkg, spec, opts) {
  const needsShrinkwrap = (!pkg || (
    pkg._hasShrinkwrap !== false &&
    !pkg._shrinkwrap
  ))
  const needsBin = !!(!pkg || (
    !pkg.bin &&
    pkg.directories &&
    pkg.directories.bin
  ))
  const needsIntegrity = !pkg || (!pkg._integrity && pkg._integrity !== false)
  const needsShasum = !pkg || (!pkg._shasum && pkg._shasum !== false)
  const needsHash = needsIntegrity || needsShasum
  const needsManifest = !pkg || !pkg.name
  const needsExtract = needsShrinkwrap || needsBin || needsManifest
  if (!needsShrinkwrap && !needsBin && !needsHash && !needsManifest) {
    return BB.resolve({})
  } else {
    opts = optCheck(opts)
    const tarStream = fetchFromManifest(pkg, spec, opts)
    const extracted = needsExtract && new tar.Parse()
    return BB.join(
      needsShrinkwrap && jsonFromStream('npm-shrinkwrap.json', extracted),
      needsManifest && jsonFromStream('package.json', extracted),
      needsBin && getPaths(extracted),
      needsHash && ssri.fromStream(tarStream, {algorithms: ['sha1', 'sha512']}),
      needsExtract && pipe(tarStream, extracted),
      (sr, mani, paths, hash) => {
        if (needsManifest && !mani) {
          const err = new Error(`Non-registry package missing package.json: ${spec}.`)
          err.code = 'ENOPACKAGEJSON'
          throw err
        }
        const extraProps = mani || {}
        delete extraProps._resolved
        // drain out the rest of the tarball
        tarStream.resume()
        // if we have directories.bin, we need to collect any matching files
        // to add to bin
        if (paths && paths.length) {
          const dirBin = mani
            ? (mani && mani.directories && mani.directories.bin)
            : (pkg && pkg.directories && pkg.directories.bin)
          if (dirBin) {
            extraProps.bin = {}
            paths.forEach(filePath => {
              if (minimatch(filePath, dirBin + '/**')) {
                const relative = path.relative(dirBin, filePath)
                if (relative && relative[0] !== '.') {
                  extraProps.bin[path.basename(relative)] = path.join(dirBin, relative)
                }
              }
            })
          }
        }
        return Object.assign(extraProps, {
          _shrinkwrap: sr,
          _resolved: (mani && mani._resolved) ||
          (pkg && pkg._resolved) ||
          spec.fetchSpec,
          _integrity: needsIntegrity && hash && hash.sha512 && hash.sha512[0].toString(),
          _shasum: needsShasum && hash && hash.sha1 && hash.sha1[0].hexDigest()
        })
      }
    )
  }
}

function jsonFromStream (filename, dataStream) {
  return BB.fromNode(cb => {
    dataStream.on('error', cb)
    dataStream.on('close', cb)
    dataStream.on('entry', entry => {
      const filePath = entry.header.path.replace(/[^/]+\//, '')
      if (filePath !== filename) {
        entry.resume()
      } else {
        let data = ''
        entry.on('error', cb)
        finished(entry).then(() => {
          try {
            cb(null, JSON.parse(data))
          } catch (err) {
            cb(err)
          }
        }, err => {
          cb(err)
        })
        entry.on('data', d => { data += d })
      }
    })
  })
}

function getPaths (dataStream) {
  return BB.fromNode(cb => {
    let paths = []
    dataStream.on('error', cb)
    dataStream.on('close', () => cb(null, paths))
    dataStream.on('entry', function handler (entry) {
      const filePath = entry.header.path.replace(/[^/]+\//, '')
      entry.resume()
      paths.push(filePath)
    })
  })
}

function finalKey (pkg, spec) {
  if (pkg && pkg._uniqueResolved) {
    // git packages have a unique, identifiable id, but no tar sha
    return cacheKey(`${spec.type}-manifest`, pkg._uniqueResolved)
  } else {
    return (
      pkg && pkg._integrity &&
      cacheKey(
        `${spec.type}-manifest`,
        `${pkg._resolved}:${ssri.stringify(pkg._integrity)}`
      )
    )
  }
}
