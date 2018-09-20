'use strict'

const BB = require('bluebird')

const binLink = require('bin-links')
const buildLogicalTree = require('npm-logical-tree')
const extract = require('./lib/extract.js')
const fs = require('graceful-fs')
const getPrefix = require('find-npm-prefix')
const lifecycle = require('npm-lifecycle')
const lockVerify = require('lock-verify')
const mkdirp = BB.promisify(require('mkdirp'))
const npa = require('npm-package-arg')
const path = require('path')
const readPkgJson = BB.promisify(require('read-package-json'))
const rimraf = BB.promisify(require('rimraf'))

const readFileAsync = BB.promisify(fs.readFile)
const statAsync = BB.promisify(fs.stat)
const symlinkAsync = BB.promisify(fs.symlink)
const writeFileAsync = BB.promisify(fs.writeFile)

class Installer {
  constructor (opts) {
    this.opts = opts
    this.config = opts.config

    // Stats
    this.startTime = Date.now()
    this.runTime = 0
    this.timings = { scripts: 0 }
    this.pkgCount = 0

    // Misc
    this.log = this.opts.log || require('./lib/silentlog.js')
    this.pkg = null
    this.tree = null
    this.failedDeps = new Set()
  }

  timedStage (name) {
    const start = Date.now()
    return BB.resolve(this[name].apply(this, [].slice.call(arguments, 1)))
      .tap(() => {
        this.timings[name] = Date.now() - start
        this.log.info(name, `Done in ${this.timings[name] / 1000}s`)
      })
  }

  run () {
    return this.timedStage('prepare')
      .then(() => this.timedStage('extractTree', this.tree))
      .then(() => this.timedStage('updateJson', this.tree))
      .then(pkgJsons => this.timedStage('buildTree', this.tree, pkgJsons))
      .then(() => this.timedStage('garbageCollect', this.tree))
      .then(() => this.timedStage('runScript', 'prepublish', this.pkg, this.prefix))
      .then(() => this.timedStage('runScript', 'prepare', this.pkg, this.prefix))
      .then(() => this.timedStage('teardown'))
      .then(() => {
        this.runTime = Date.now() - this.startTime
        this.log.info(
          'run-scripts',
          `total script time: ${this.timings.scripts / 1000}s`
        )
        this.log.info(
          'run-time',
          `total run time: ${this.runTime / 1000}s`
        )
      })
      .catch(err => {
        this.timedStage('teardown')
        if (err.message.match(/aggregate error/)) {
          throw err[0]
        } else {
          throw err
        }
      })
      .then(() => this)
  }

  prepare () {
    this.log.info('prepare', 'initializing installer')
    this.log.level = this.config.get('loglevel')
    this.log.verbose('prepare', 'starting workers')
    extract.startWorkers()

    return (
      this.config.get('prefix') && this.config.get('global')
        ? BB.resolve(this.config.get('prefix'))
        // There's some Special™ logic around the `--prefix` config when it
        // comes from a config file or env vs when it comes from the CLI
        : process.argv.some(arg => arg.match(/^\s*--prefix\s*/i))
          ? BB.resolve(this.config.get('prefix'))
          : getPrefix(process.cwd())
    )
      .then(prefix => {
        this.prefix = prefix
        this.log.verbose('prepare', 'installation prefix: ' + prefix)
        return BB.join(
          readJson(prefix, 'package.json'),
          readJson(prefix, 'package-lock.json', true),
          readJson(prefix, 'npm-shrinkwrap.json', true),
          (pkg, lock, shrink) => {
            if (shrink) {
              this.log.verbose('prepare', 'using npm-shrinkwrap.json')
            } else if (lock) {
              this.log.verbose('prepare', 'using package-lock.json')
            }
            pkg._shrinkwrap = shrink || lock
            this.pkg = pkg
          }
        )
      })
      .then(() => statAsync(
        path.join(this.prefix, 'node_modules')
      ).catch(err => { if (err.code !== 'ENOENT') { throw err } }))
      .then(stat => {
        stat && this.log.warn(
          'prepare', 'removing existing node_modules/ before installation'
        )
        return BB.join(
          this.checkLock(),
          stat && rimraf(path.join(this.prefix, 'node_modules'))
        )
      }).then(() => {
      // This needs to happen -after- we've done checkLock()
        this.tree = buildLogicalTree(this.pkg, this.pkg._shrinkwrap)
        this.log.silly('tree', this.tree)
        this.expectedTotal = 0
        this.tree.forEach((dep, next) => {
          this.expectedTotal++
          next()
        })
      })
  }

  teardown () {
    this.log.verbose('teardown', 'shutting down workers.')
    return extract.stopWorkers()
  }

  checkLock () {
    this.log.verbose('checkLock', 'verifying package-lock data')
    const pkg = this.pkg
    const prefix = this.prefix
    if (!pkg._shrinkwrap || !pkg._shrinkwrap.lockfileVersion) {
      return BB.reject(
        new Error(`cipm can only install packages with an existing package-lock.json or npm-shrinkwrap.json with lockfileVersion >= 1. Run an install with npm@5 or later to generate it, then try again.`)
      )
    }
    return lockVerify(prefix).then(result => {
      if (result.status) {
        result.warnings.forEach(w => this.log.warn('lockfile', w))
      } else {
        throw new Error(
          'cipm can only install packages when your package.json and package-lock.json or ' +
          'npm-shrinkwrap.json are in sync. Please update your lock file with `npm install` ' +
          'before continuing.\n\n' +
          result.warnings.map(w => 'Warning: ' + w).join('\n') + '\n' +
          result.errors.join('\n') + '\n'
        )
      }
    }).catch(err => {
      throw err
    })
  }

  extractTree (tree) {
    this.log.verbose('extractTree', 'extracting dependencies to node_modules/')
    const cg = this.log.newItem('extractTree', this.expectedTotal)
    return tree.forEachAsync((dep, next) => {
      if (!this.checkDepEnv(dep)) { return }
      const depPath = dep.path(this.prefix)
      const spec = npa.resolve(dep.name, dep.version, this.prefix)
      if (dep.isRoot) {
        return next()
      } else if (spec.type === 'directory') {
        const relative = path.relative(path.dirname(depPath), spec.fetchSpec)
        this.log.silly('extractTree', `${dep.name}@${spec.fetchSpec} -> ${depPath} (symlink)`)
        return mkdirp(path.dirname(depPath))
          .then(() => symlinkAsync(relative, depPath, 'junction'))
          .catch(
            () => rimraf(depPath)
              .then(() => symlinkAsync(relative, depPath, 'junction'))
          ).then(() => next())
          .then(() => {
            this.pkgCount++
            cg.completeWork(1)
          })
      } else {
        this.log.silly('extractTree', `${dep.name}@${dep.version} -> ${depPath}`)
        return (
          dep.bundled
            ? statAsync(path.join(depPath, 'package.json')).catch(err => {
              if (err.code !== 'ENOENT') { throw err }
            })
            : BB.resolve(false)
        )
          .then(wasBundled => {
          // Don't extract if a bundled dep is actually present
            if (wasBundled) {
              cg.completeWork(1)
              return next()
            } else {
              return BB.resolve(extract.child(
                dep.name, dep, depPath, this.config, this.opts
              ))
                .then(() => cg.completeWork(1))
                .then(() => { this.pkgCount++ })
                .then(next)
            }
          })
      }
    }, {concurrency: 50, Promise: BB})
      .then(() => cg.finish())
  }

  checkDepEnv (dep) {
    const includeDev = (
      // Covers --dev and --development (from npm config itself)
      this.config.get('dev') ||
      (
        !/^prod(uction)?$/.test(this.config.get('only')) &&
        !this.config.get('production')
      ) ||
      /^dev(elopment)?$/.test(this.config.get('only')) ||
      /^dev(elopment)?$/.test(this.config.get('also'))
    )
    const includeProd = !/^dev(elopment)?$/.test(this.config.get('only'))
    return (dep.dev && includeDev) || (!dep.dev && includeProd)
  }

  updateJson (tree) {
    this.log.verbose('updateJson', 'updating json deps to include _from')
    const pkgJsons = new Map()
    return tree.forEachAsync((dep, next) => {
      if (!this.checkDepEnv(dep)) { return }
      const spec = npa.resolve(dep.name, dep.version)
      const depPath = dep.path(this.prefix)
      return next()
        .then(() => readJson(depPath, 'package.json'))
        .then(pkg => (spec.registry || spec.type === 'directory')
          ? pkg
          : this.updateFromField(dep, pkg).then(() => pkg)
        )
        .then(pkg => (pkg.scripts && pkg.scripts.install)
          ? pkg
          : this.updateInstallScript(dep, pkg).then(() => pkg)
        )
        .tap(pkg => { pkgJsons.set(dep, pkg) })
    }, {concurrency: 100, Promise: BB})
      .then(() => pkgJsons)
  }

  buildTree (tree, pkgJsons) {
    this.log.verbose('buildTree', 'finalizing tree and running scripts')
    return tree.forEachAsync((dep, next) => {
      if (!this.checkDepEnv(dep)) { return }
      const spec = npa.resolve(dep.name, dep.version)
      const depPath = dep.path(this.prefix)
      const pkg = pkgJsons.get(dep)
      this.log.silly('buildTree', `linking ${spec}`)
      return this.runScript('preinstall', pkg, depPath)
        .then(next) // build children between preinstall and binLink
      // Don't link root bins
        .then(() => {
          if (
            dep.isRoot ||
          !(pkg.bin || pkg.man || (pkg.directories && pkg.directories.bin))
          ) {
          // We skip the relatively expensive readPkgJson if there's no way
          // we'll actually be linking any bins or mans
            return
          }
          return readPkgJson(path.join(depPath, 'package.json'))
            .then(pkg => binLink(pkg, depPath, false, {
              force: this.config.get('force'),
              ignoreScripts: this.config.get('ignore-scripts'),
              log: Object.assign({}, this.log, { info: () => {} }),
              name: pkg.name,
              pkgId: pkg.name + '@' + pkg.version,
              prefix: this.prefix,
              prefixes: [this.prefix],
              umask: this.config.get('umask')
            }), e => {
              this.log.verbose('buildTree', `error linking ${spec}: ${e.message} ${e.stack}`)
            })
        })
        .then(() => this.runScript('install', pkg, depPath))
        .then(() => this.runScript('postinstall', pkg, depPath))
        .then(() => this)
        .catch(e => {
          if (dep.optional) {
            this.failedDeps.add(dep)
          } else {
            throw e
          }
        })
    }, {concurrency: 1, Promise: BB})
  }

  updateFromField (dep, pkg) {
    const depPath = dep.path(this.prefix)
    const depPkgPath = path.join(depPath, 'package.json')
    const parent = dep.requiredBy.values().next().value
    return readJson(parent.path(this.prefix), 'package.json')
      .then(ppkg =>
        (ppkg.dependencies && ppkg.dependencies[dep.name]) ||
      (ppkg.devDependencies && ppkg.devDependencies[dep.name]) ||
      (ppkg.optionalDependencies && ppkg.optionalDependencies[dep.name])
      )
      .then(from => npa.resolve(dep.name, from))
      .then(from => { pkg._from = from.toString() })
      .then(() => writeFileAsync(depPkgPath, JSON.stringify(pkg, null, 2)))
      .then(pkg)
  }

  updateInstallScript (dep, pkg) {
    const depPath = dep.path(this.prefix)
    return statAsync(path.join(depPath, 'binding.gyp'))
      .catch(err => { if (err.code !== 'ENOENT') { throw err } })
      .then(stat => {
        if (stat) {
          if (!pkg.scripts) {
            pkg.scripts = {}
          }
          pkg.scripts.install = 'node-gyp rebuild'
        }
      })
      .then(pkg)
  }

  // A cute little mark-and-sweep collector!
  garbageCollect (tree) {
    if (!this.failedDeps.size) { return }
    return sweep(
      tree,
      this.prefix,
      mark(tree, this.failedDeps)
    )
      .then(purged => {
        this.purgedDeps = purged
        this.pkgCount -= purged.size
      })
  }

  runScript (stage, pkg, pkgPath) {
    const start = Date.now()
    if (!this.config.get('ignore-scripts')) {
      // TODO(mikesherov): remove pkg._id when npm-lifecycle no longer relies on it
      pkg._id = pkg.name + '@' + pkg.version
      const opts = this.config.toLifecycle()
      return BB.resolve(lifecycle(pkg, stage, pkgPath, opts))
        .tap(() => { this.timings.scripts += Date.now() - start })
    }
    return BB.resolve()
  }
}
module.exports = Installer
module.exports.CipmConfig = require('./lib/config/npm-config.js').CipmConfig

function mark (tree, failed) {
  const liveDeps = new Set()
  tree.forEach((dep, next) => {
    if (!failed.has(dep)) {
      liveDeps.add(dep)
      next()
    }
  })
  return liveDeps
}

function sweep (tree, prefix, liveDeps) {
  const purged = new Set()
  return tree.forEachAsync((dep, next) => {
    return next().then(() => {
      if (
        !dep.isRoot && // never purge root! 🙈
        !liveDeps.has(dep) &&
        !purged.has(dep)
      ) {
        purged.add(dep)
        return rimraf(dep.path(prefix))
      }
    })
  }, {concurrency: 50, Promise: BB}).then(() => purged)
}

function stripBOM (str) {
  return str.replace(/^\uFEFF/, '')
}

module.exports._readJson = readJson
function readJson (jsonPath, name, ignoreMissing) {
  return readFileAsync(path.join(jsonPath, name), 'utf8')
    .then(str => JSON.parse(stripBOM(str)))
    .catch({code: 'ENOENT'}, err => {
      if (!ignoreMissing) {
        throw err
      }
    })
}
