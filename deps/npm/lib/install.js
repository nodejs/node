'use strict'
// npm install <pkg> <pkg> <pkg>
//
// See doc/cli/npm-install.md for more description
//
// Managing contexts...
// there's a lot of state associated with an "install" operation, including
// packages that are already installed, parent packages, current shrinkwrap, and
// so on. We maintain this state in a "context" object that gets passed around.
// every time we dive into a deeper node_modules folder, the "family" list that
// gets passed along uses the previous "family" list as its __proto__.  Any
// "resolved precise dependency" things that aren't already on this object get
// added, and then that's passed to the next generation of installation.

module.exports = install
module.exports.Installer = Installer

var usage = require('./utils/usage')

install.usage = usage(
  'install',
  '\nnpm install (with no args, in package dir)' +
  '\nnpm install [<@scope>/]<pkg>' +
  '\nnpm install [<@scope>/]<pkg>@<tag>' +
  '\nnpm install [<@scope>/]<pkg>@<version>' +
  '\nnpm install [<@scope>/]<pkg>@<version range>' +
  '\nnpm install <folder>' +
  '\nnpm install <tarball file>' +
  '\nnpm install <tarball url>' +
  '\nnpm install <git:// url>' +
  '\nnpm install <github username>/<github project>',
  '[--save-prod|--save-dev|--save-optional] [--save-exact] [--no-save]'
)

install.completion = function (opts, cb) {
  validate('OF', arguments)
  // install can complete to a folder with a package.json, or any package.
  // if it has a slash, then it's gotta be a folder
  // if it starts with https?://, then just give up, because it's a url
  if (/^https?:\/\//.test(opts.partialWord)) {
    // do not complete to URLs
    return cb(null, [])
  }

  if (/\//.test(opts.partialWord)) {
    // Complete fully to folder if there is exactly one match and it
    // is a folder containing a package.json file.  If that is not the
    // case we return 0 matches, which will trigger the default bash
    // complete.
    var lastSlashIdx = opts.partialWord.lastIndexOf('/')
    var partialName = opts.partialWord.slice(lastSlashIdx + 1)
    var partialPath = opts.partialWord.slice(0, lastSlashIdx)
    if (partialPath === '') partialPath = '/'

    var annotatePackageDirMatch = function (sibling, cb) {
      var fullPath = path.join(partialPath, sibling)
      if (sibling.slice(0, partialName.length) !== partialName) {
        return cb(null, null) // not name match
      }
      fs.readdir(fullPath, function (err, contents) {
        if (err) return cb(null, { isPackage: false })

        cb(
          null,
          {
            fullPath: fullPath,
            isPackage: contents.indexOf('package.json') !== -1
          }
        )
      })
    }

    return fs.readdir(partialPath, function (err, siblings) {
      if (err) return cb(null, []) // invalid dir: no matching

      asyncMap(siblings, annotatePackageDirMatch, function (err, matches) {
        if (err) return cb(err)

        var cleaned = matches.filter(function (x) { return x !== null })
        if (cleaned.length !== 1) return cb(null, [])
        if (!cleaned[0].isPackage) return cb(null, [])

        // Success - only one match and it is a package dir
        return cb(null, [cleaned[0].fullPath])
      })
    })
  }

  // FIXME: there used to be registry completion here, but it stopped making
  // sense somewhere around 50,000 packages on the registry
  cb()
}

// system packages
var fs = require('fs')
var path = require('path')

// dependencies
var log = require('npmlog')
var readPackageTree = require('read-package-tree')
var readPackageJson = require('read-package-json')
var chain = require('slide').chain
var asyncMap = require('slide').asyncMap
var archy = require('archy')
var mkdirp = require('mkdirp')
var rimraf = require('rimraf')
var iferr = require('iferr')
var validate = require('aproba')
var uniq = require('lodash.uniq')
var Bluebird = require('bluebird')

// npm internal utils
var npm = require('./npm.js')
var locker = require('./utils/locker.js')
var lock = locker.lock
var unlock = locker.unlock
var parseJSON = require('./utils/parse-json.js')
var output = require('./utils/output.js')
var saveMetrics = require('./utils/metrics.js').save

// install specific libraries
var copyTree = require('./install/copy-tree.js')
var readShrinkwrap = require('./install/read-shrinkwrap.js')
var computeMetadata = require('./install/deps.js').computeMetadata
var prefetchDeps = require('./install/deps.js').prefetchDeps
var loadDeps = require('./install/deps.js').loadDeps
var loadDevDeps = require('./install/deps.js').loadDevDeps
var getAllMetadata = require('./install/deps.js').getAllMetadata
var loadRequestedDeps = require('./install/deps.js').loadRequestedDeps
var loadExtraneous = require('./install/deps.js').loadExtraneous
var diffTrees = require('./install/diff-trees.js')
var checkPermissions = require('./install/check-permissions.js')
var decomposeActions = require('./install/decompose-actions.js')
var validateTree = require('./install/validate-tree.js')
var validateArgs = require('./install/validate-args.js')
var saveRequested = require('./install/save.js').saveRequested
var saveShrinkwrap = require('./install/save.js').saveShrinkwrap
var getSaveType = require('./install/save.js').getSaveType
var doSerialActions = require('./install/actions.js').doSerial
var doReverseSerialActions = require('./install/actions.js').doReverseSerial
var doParallelActions = require('./install/actions.js').doParallel
var doOneAction = require('./install/actions.js').doOne
var removeObsoleteDep = require('./install/deps.js').removeObsoleteDep
var removeExtraneous = require('./install/deps.js').removeExtraneous
var computeVersionSpec = require('./install/deps.js').computeVersionSpec
var packageId = require('./utils/package-id.js')
var moduleName = require('./utils/module-name.js')
var errorMessage = require('./utils/error-message.js')
var isExtraneous = require('./install/is-extraneous.js')

function unlockCB (lockPath, name, cb) {
  validate('SSF', arguments)
  return function (installEr) {
    var args = arguments
    try {
      unlock(lockPath, name, reportErrorAndReturn)
    } catch (unlockEx) {
      process.nextTick(function () {
        reportErrorAndReturn(unlockEx)
      })
    }
    function reportErrorAndReturn (unlockEr) {
      if (installEr) {
        if (unlockEr && unlockEr.code !== 'ENOTLOCKED') {
          log.warn('unlock' + name, unlockEr)
        }
        return cb.apply(null, args)
      }
      if (unlockEr) return cb(unlockEr)
      return cb.apply(null, args)
    }
  }
}

function install (where, args, cb) {
  if (!cb) {
    cb = args
    args = where
    where = null
  }
  var globalTop = path.resolve(npm.globalDir, '..')
  if (!where) {
    where = npm.config.get('global')
          ? globalTop
          : npm.prefix
  }
  validate('SAF', [where, args, cb])
  // the /path/to/node_modules/..
  var dryrun = !!npm.config.get('dry-run')

  if (npm.config.get('dev')) {
    log.warn('install', 'Usage of the `--dev` option is deprecated. Use `--only=dev` instead.')
  }

  if (where === globalTop && !args.length) {
    args = ['.']
  }
  args = args.filter(function (a) {
    return path.resolve(a) !== npm.prefix
  })

  new Installer(where, dryrun, args).run(cb)
}

function Installer (where, dryrun, args, opts) {
  validate('SBA|SBAO', arguments)
  if (!opts) opts = {}
  this.where = where
  this.dryrun = dryrun
  this.args = args
  // fakechildren are children created from the lockfile and lack relationship data
  // the only exist when the tree does not match the lockfile
  // this is fine when doing full tree installs/updates but not ok when modifying only
  // a few deps via `npm install` or `npm uninstall`.
  this.currentTree = null
  this.idealTree = null
  this.differences = []
  this.todo = []
  this.progress = {}
  this.noPackageJsonOk = !!args.length
  this.topLevelLifecycles = !args.length

  this.autoPrune = npm.config.get('package-lock')

  const dev = npm.config.get('dev')
  const only = npm.config.get('only')
  const onlyProd = /^prod(uction)?$/.test(only)
  const onlyDev = /^dev(elopment)?$/.test(only)
  const prod = npm.config.get('production')
  this.dev = opts.dev != null ? opts.dev : dev || (!onlyProd && !prod) || onlyDev
  this.prod = opts.prod != null ? opts.prod : !onlyDev

  this.packageLockOnly = opts.packageLockOnly != null
    ? opts.packageLockOnly : npm.config.get('package-lock-only')
  this.rollback = opts.rollback != null ? opts.rollback : npm.config.get('rollback')
  this.link = opts.link != null ? opts.link : npm.config.get('link')
  this.saveOnlyLock = opts.saveOnlyLock
  this.global = opts.global != null ? opts.global : this.where === path.resolve(npm.globalDir, '..')
  this.started = Date.now()
}
Installer.prototype = {}

Installer.prototype.run = function (_cb) {
  validate('F|', arguments)

  var result
  var cb
  if (_cb) {
    cb = function (err) {
      saveMetrics(!err)
      return _cb.apply(this, arguments)
    }
  } else {
    result = new Promise((resolve, reject) => {
      cb = (err, value) => err ? reject(err) : resolve(value)
    })
  }
  // FIXME: This is bad and I should feel bad.
  // lib/install needs to have some way of sharing _limited_
  // state with the things it calls. Passing the object is too
  // much. The global config is WAY too much. =( =(
  // But not having this is gonna break linked modules in
  // subtle stupid ways, and refactoring all this code isn't
  // the right thing to do just yet.
  if (this.global) {
    var prevGlobal = npm.config.get('global')
    npm.config.set('global', true)
    var next = cb
    cb = function () {
      npm.config.set('global', prevGlobal)
      next.apply(null, arguments)
    }
  }

  var installSteps = []
  var postInstallSteps = []
  if (!this.dryrun) {
    installSteps.push(
      [this.newTracker(log, 'runTopLevelLifecycles', 2)],
      [this, this.runPreinstallTopLevelLifecycles])
  }
  installSteps.push(
    [this.newTracker(log, 'loadCurrentTree', 4)],
    [this, this.loadCurrentTree],
    [this, this.finishTracker, 'loadCurrentTree'],

    [this.newTracker(log, 'loadIdealTree', 12)],
    [this, this.loadIdealTree],
    [this, this.finishTracker, 'loadIdealTree'],

    [this, this.debugTree, 'currentTree', 'currentTree'],
    [this, this.debugTree, 'idealTree', 'idealTree'],

    [this.newTracker(log, 'generateActionsToTake')],
    [this, this.generateActionsToTake],
    [this, this.finishTracker, 'generateActionsToTake'],

    [this, this.debugActions, 'diffTrees', 'differences'],
    [this, this.debugActions, 'decomposeActions', 'todo'])

  if (this.packageLockOnly) {
    postInstallSteps.push(
      [this, this.saveToDependencies])
  } else if (!this.dryrun) {
    installSteps.push(
      [this.newTracker(log, 'executeActions', 8)],
      [this, this.executeActions],
      [this, this.finishTracker, 'executeActions'])
    var node_modules = path.resolve(this.where, 'node_modules')
    var staging = path.resolve(node_modules, '.staging')
    postInstallSteps.push(
      [this.newTracker(log, 'rollbackFailedOptional', 1)],
      [this, this.rollbackFailedOptional, staging, this.todo],
      [this, this.finishTracker, 'rollbackFailedOptional'],
      [this, this.commit, staging, this.todo],

      [this, this.runPostinstallTopLevelLifecycles],
      [this, this.finishTracker, 'runTopLevelLifecycles']
    )
    if (getSaveType()) {
      postInstallSteps.push(
        // this is necessary as we don't fill in `dependencies` and `devDependencies` in deps loaded from shrinkwrap
        // until after we extract them
        [this, (next) => { computeMetadata(this.idealTree); next() }],
        [this, this.pruneIdealTree],
        [this, this.debugLogicalTree, 'saveTree', 'idealTree'],
        [this, this.saveToDependencies])
    }
  }
  postInstallSteps.push(
    [this, this.printWarnings],
    [this, this.printInstalled])

  var self = this
  chain(installSteps, function (installEr) {
    if (installEr) self.failing = true
    chain(postInstallSteps, function (postInstallEr) {
      if (installEr && postInstallEr) {
        var msg = errorMessage(postInstallEr)
        msg.summary.forEach(function (logline) {
          log.warn.apply(log, logline)
        })
        msg.detail.forEach(function (logline) {
          log.verbose.apply(log, logline)
        })
      }
      cb(installEr || postInstallEr, self.getInstalledModules(), self.idealTree)
    })
  })
  return result
}

Installer.prototype.loadArgMetadata = function (next) {
  getAllMetadata(this.args, this.currentTree, process.cwd(), iferr(next, (args) => {
    this.args = args
    next()
  }))
}

Installer.prototype.newTracker = function (tracker, name, size) {
  validate('OS', [tracker, name])
  if (size) validate('N', [size])
  this.progress[name] = tracker.newGroup(name, size)
  return function (next) {
    process.emit('time', 'stage:' + name)
    next()
  }
}

Installer.prototype.finishTracker = function (name, cb) {
  validate('SF', arguments)
  process.emit('timeEnd', 'stage:' + name)
  cb()
}

Installer.prototype.loadCurrentTree = function (cb) {
  validate('F', arguments)
  log.silly('install', 'loadCurrentTree')
  var todo = []
  if (this.global) {
    todo.push([this, this.readGlobalPackageData])
  } else {
    todo.push([this, this.readLocalPackageData])
  }
  todo.push([this, this.normalizeCurrentTree])
  chain(todo, cb)
}

var createNode = require('./install/node.js').create
var flatNameFromTree = require('./install/flatten-tree.js').flatNameFromTree
Installer.prototype.normalizeCurrentTree = function (cb) {
  this.currentTree.isTop = true
  normalizeTree(this.currentTree)
  // If the user didn't have a package.json then fill in deps with what was on disk
  if (this.currentTree.error) {
    for (let child of this.currentTree.children) {
      if (!child.fakeChild && isExtraneous(child)) {
        this.currentTree.package.dependencies[child.package.name] = computeVersionSpec(this.currentTree, child)
      }
    }
  }
  computeMetadata(this.currentTree)
  return cb()

  function normalizeTree (tree, seen) {
    if (!seen) seen = new Set()
    if (seen.has(tree)) return
    seen.add(tree)
    createNode(tree)
    tree.location = flatNameFromTree(tree)
    tree.children.forEach((child) => normalizeTree(child, seen))
  }
}

Installer.prototype.loadIdealTree = function (cb) {
  validate('F', arguments)
  log.silly('install', 'loadIdealTree')

  chain([
    [this.newTracker(this.progress.loadIdealTree, 'loadIdealTree:cloneCurrentTree')],
    [this, this.cloneCurrentTreeToIdealTree],
    [this, this.finishTracker, 'loadIdealTree:cloneCurrentTree'],

    [this.newTracker(this.progress.loadIdealTree, 'loadIdealTree:loadShrinkwrap')],
    [this, this.loadShrinkwrap],
    [this, this.finishTracker, 'loadIdealTree:loadShrinkwrap'],

    [this.newTracker(this.progress.loadIdealTree, 'loadIdealTree:loadAllDepsIntoIdealTree', 10)],
    [this, this.loadAllDepsIntoIdealTree],
    [this, this.finishTracker, 'loadIdealTree:loadAllDepsIntoIdealTree'],
    [this, function (next) { computeMetadata(this.idealTree); next() }],
    [this, this.pruneIdealTree]
  ], cb)
}

Installer.prototype.pruneIdealTree = function (cb) {
  if (!this.idealTree) return cb()
  // if our lock file didn't have the requires field and there
  // are any fake children then forgo pruning until we have more info.
  if (!this.idealTree.hasRequiresFromLock && this.idealTree.children.some((n) => n.fakeChild)) return cb()
  const toPrune = this.idealTree.children
    .filter((child) => isExtraneous(child) && (this.autoPrune || child.removing))
    .map((n) => ({name: moduleName(n)}))
  return removeExtraneous(toPrune, this.idealTree, cb)
}

Installer.prototype.loadAllDepsIntoIdealTree = function (cb) {
  validate('F', arguments)
  log.silly('install', 'loadAllDepsIntoIdealTree')
  var saveDeps = getSaveType()

  var cg = this.progress['loadIdealTree:loadAllDepsIntoIdealTree']
  var installNewModules = !!this.args.length
  var steps = []

  if (installNewModules) {
    steps.push([validateArgs, this.idealTree, this.args])
    steps.push([loadRequestedDeps, this.args, this.idealTree, saveDeps, cg.newGroup('loadRequestedDeps')])
  } else {
    const depsToPreload = Object.assign({},
      this.dev ? this.idealTree.package.devDependencies : {},
      this.prod ? this.idealTree.package.dependencies : {}
    )
    if (this.prod || this.dev) {
      steps.push(
        [prefetchDeps, this.idealTree, depsToPreload, cg.newGroup('prefetchDeps')])
    }
    if (this.prod) {
      steps.push(
        [loadDeps, this.idealTree, cg.newGroup('loadDeps')])
    }
    if (this.dev) {
      steps.push(
        [loadDevDeps, this.idealTree, cg.newGroup('loadDevDeps')])
    }
  }
  steps.push(
    [loadExtraneous.andResolveDeps, this.idealTree, cg.newGroup('loadExtraneous')])
  chain(steps, cb)
}

Installer.prototype.generateActionsToTake = function (cb) {
  validate('F', arguments)
  log.silly('install', 'generateActionsToTake')
  var cg = this.progress.generateActionsToTake
  chain([
    [validateTree, this.idealTree, cg.newGroup('validateTree')],
    [diffTrees, this.currentTree, this.idealTree, this.differences, cg.newGroup('diffTrees')],
    [this, this.computeLinked],
    [checkPermissions, this.differences],
    [decomposeActions, this.differences, this.todo]
  ], cb)
}

Installer.prototype.computeLinked = function (cb) {
  validate('F', arguments)
  if (!this.link || this.global) return cb()
  var linkTodoList = []
  var self = this
  asyncMap(this.differences, function (action, next) {
    var cmd = action[0]
    var pkg = action[1]
    if (cmd !== 'add' && cmd !== 'update') return next()
    var isReqByTop = pkg.requiredBy.filter(function (mod) { return mod.isTop }).length
    var isReqByUser = pkg.userRequired
    var isExtraneous = pkg.requiredBy.length === 0
    if (!isReqByTop && !isReqByUser && !isExtraneous) return next()
    isLinkable(pkg, function (install, link) {
      if (install) linkTodoList.push(['global-install', pkg])
      if (link) linkTodoList.push(['global-link', pkg])
      if (install || link) removeObsoleteDep(pkg)
      next()
    })
  }, function () {
    if (linkTodoList.length === 0) return cb()
    self.differences.length = 0
    Array.prototype.push.apply(self.differences, linkTodoList)
    diffTrees(self.currentTree, self.idealTree, self.differences, log.newGroup('d2'), cb)
  })
}

function isLinkable (pkg, cb) {
  var globalPackage = path.resolve(npm.globalPrefix, 'lib', 'node_modules', moduleName(pkg))
  var globalPackageJson = path.resolve(globalPackage, 'package.json')
  fs.stat(globalPackage, function (er) {
    if (er) return cb(true, true)
    fs.readFile(globalPackageJson, function (er, data) {
      var json = parseJSON.noExceptions(data)
      cb(false, json && json.version === pkg.package.version)
    })
  })
}

Installer.prototype.executeActions = function (cb) {
  validate('F', arguments)
  log.silly('install', 'executeActions')
  var todo = this.todo
  var cg = this.progress.executeActions

  var node_modules = path.resolve(this.where, 'node_modules')
  var staging = path.resolve(node_modules, '.staging')
  var steps = []
  var trackLifecycle = cg.newGroup('lifecycle')

  cb = unlockCB(node_modules, '.staging', cb)

  steps.push(
    [doSerialActions, 'global-install', staging, todo, trackLifecycle.newGroup('global-install')],
    [lock, node_modules, '.staging'],
    [rimraf, staging],
    [doParallelActions, 'extract', staging, todo, cg.newGroup('extract', 100)],
    [doReverseSerialActions, 'unbuild', staging, todo, cg.newGroup('unbuild')],
    [doSerialActions, 'remove', staging, todo, cg.newGroup('remove')],
    [doSerialActions, 'move', staging, todo, cg.newGroup('move')],
    [doSerialActions, 'finalize', staging, todo, cg.newGroup('finalize')],
    [doParallelActions, 'refresh-package-json', staging, todo, cg.newGroup('refresh-package-json')],
    [doParallelActions, 'preinstall', staging, todo, trackLifecycle.newGroup('preinstall')],
    [doSerialActions, 'build', staging, todo, trackLifecycle.newGroup('build')],
    [doSerialActions, 'global-link', staging, todo, trackLifecycle.newGroup('global-link')],
    [doParallelActions, 'update-linked', staging, todo, trackLifecycle.newGroup('update-linked')],
    [doSerialActions, 'install', staging, todo, trackLifecycle.newGroup('install')],
    [doSerialActions, 'postinstall', staging, todo, trackLifecycle.newGroup('postinstall')])

  var self = this
  chain(steps, function (er) {
    if (!er || self.rollback) {
      rimraf(staging, function () { cb(er) })
    } else {
      cb(er)
    }
  })
}

Installer.prototype.rollbackFailedOptional = function (staging, actionsToRun, cb) {
  if (!this.rollback) return cb()
  var failed = uniq(actionsToRun.map(function (action) {
    return action[1]
  }).filter(function (pkg) {
    return pkg.failed && pkg.rollback
  }))
  var top = this.currentTree && this.currentTree.path
  Bluebird.map(failed, (pkg) => {
    return Bluebird.map(pkg.rollback, (rollback) => rollback(top, staging, pkg))
  }).asCallback(cb)
}

Installer.prototype.commit = function (staging, actionsToRun, cb) {
  var toCommit = actionsToRun.map(function (action) { return action[1] }).filter(function (pkg) { return !pkg.failed && pkg.commit })
  asyncMap(toCommit, function (pkg, next) {
    asyncMap(pkg.commit, function (commit, done) {
      commit(staging, pkg, done)
    }, function () {
      pkg.commit = []
      next.apply(null, arguments)
    })
  }, cb)
}

Installer.prototype.runPreinstallTopLevelLifecycles = function (cb) {
  validate('F', arguments)
  if (this.failing) return cb()
  if (!this.topLevelLifecycles) return cb()
  log.silly('install', 'runPreinstallTopLevelLifecycles')

  readPackageJson(path.join(this.where, 'package.json'), log, false, (err, data) => {
    if (err) return cb()
    this.currentTree = createNode({
      isTop: true,
      package: data,
      path: this.where
    })
    doOneAction('preinstall', this.where, this.currentTree, log.newGroup('preinstall:.'), cb)
  })
}

Installer.prototype.runPostinstallTopLevelLifecycles = function (cb) {
  validate('F', arguments)
  if (this.failing) return cb()
  if (!this.topLevelLifecycles) return cb()
  log.silly('install', 'runPostinstallTopLevelLifecycles')
  var steps = []
  var trackLifecycle = this.progress.runTopLevelLifecycles

  steps.push(
    [doOneAction, 'build', this.idealTree.path, this.idealTree, trackLifecycle.newGroup('build:.')],
    [doOneAction, 'install', this.idealTree.path, this.idealTree, trackLifecycle.newGroup('install:.')],
    [doOneAction, 'postinstall', this.idealTree.path, this.idealTree, trackLifecycle.newGroup('postinstall:.')])
  if (this.dev) {
    steps.push(
      [doOneAction, 'prepare', this.idealTree.path, this.idealTree, trackLifecycle.newGroup('prepare')])
  }
  chain(steps, cb)
}

Installer.prototype.saveToDependencies = function (cb) {
  validate('F', arguments)
  if (this.failing) return cb()
  log.silly('install', 'saveToDependencies')
  if (this.saveOnlyLock) {
    saveShrinkwrap(this.idealTree, cb)
  } else {
    saveRequested(this.idealTree, cb)
  }
}

Installer.prototype.readGlobalPackageData = function (cb) {
  validate('F', arguments)
  log.silly('install', 'readGlobalPackageData')
  var self = this
  this.loadArgMetadata(iferr(cb, function () {
    mkdirp(self.where, iferr(cb, function () {
      var pkgs = {}
      self.args.forEach(function (pkg) {
        pkgs[pkg.name] = true
      })
      readPackageTree(self.where, function (ctx, kid) { return ctx.parent || pkgs[kid] }, iferr(cb, function (currentTree) {
        self.currentTree = currentTree
        return cb()
      }))
    }))
  }))
}

Installer.prototype.readLocalPackageData = function (cb) {
  validate('F', arguments)
  log.silly('install', 'readLocalPackageData')
  var self = this
  mkdirp(this.where, iferr(cb, function () {
    readPackageTree(self.where, iferr(cb, function (currentTree) {
      self.currentTree = currentTree
      self.currentTree.warnings = []
      if (currentTree.error && currentTree.error.code === 'EJSONPARSE') {
        return cb(currentTree.error)
      }
      if (!self.noPackageJsonOk && !currentTree.package) {
        log.error('install', "Couldn't read dependencies")
        var er = new Error("ENOENT, open '" + path.join(self.where, 'package.json') + "'")
        er.code = 'ENOPACKAGEJSON'
        er.errno = 34
        return cb(er)
      }
      if (!currentTree.package) currentTree.package = {}
      readShrinkwrap(currentTree, function (err) {
        if (err) {
          cb(err)
        } else {
          self.loadArgMetadata(cb)
        }
      })
    }))
  }))
}

Installer.prototype.cloneCurrentTreeToIdealTree = function (cb) {
  validate('F', arguments)
  log.silly('install', 'cloneCurrentTreeToIdealTree')

  this.idealTree = copyTree(this.currentTree)
  this.idealTree.warnings = []
  cb()
}

Installer.prototype.loadShrinkwrap = function (cb) {
  validate('F', arguments)
  log.silly('install', 'loadShrinkwrap')
  readShrinkwrap.andInflate(this.idealTree, iferr(cb, () => {
    computeMetadata(this.idealTree)
    cb()
  }))
}

Installer.prototype.getInstalledModules = function () {
  return this.differences.filter(function (action) {
    var mutation = action[0]
    return (mutation === 'add' || mutation === 'update')
  }).map(function (action) {
    var child = action[1]
    return [child.package._id, child.path]
  })
}

Installer.prototype.printWarnings = function (cb) {
  if (!this.idealTree) return cb()

  var self = this
  var warned = false
  this.idealTree.warnings.forEach(function (warning) {
    if (warning.code === 'EPACKAGEJSON' && self.global) return
    if (warning.code === 'ENOTDIR') return
    warned = true
    var msg = errorMessage(warning)
    msg.summary.forEach(function (logline) {
      log.warn.apply(log, logline)
    })
    msg.detail.forEach(function (logline) {
      log.verbose.apply(log, logline)
    })
  })
  if (warned && log.levels[npm.config.get('loglevel')] <= log.levels.warn) console.error()
  cb()
}

Installer.prototype.printInstalled = function (cb) {
  validate('F', arguments)
  if (this.failing) return cb()
  log.silly('install', 'printInstalled')
  const diffs = this.differences
  if (!this.idealTree.error && this.idealTree.removedChildren) {
    const deps = this.currentTree.package.dependencies || {}
    const dev = this.currentTree.package.devDependencies || {}
    this.idealTree.removedChildren.forEach((r) => {
      if (diffs.some((d) => d[0] === 'remove' && d[1].path === r.path)) return
      if (!deps[moduleName(r)] && !dev[moduleName(r)]) return
      diffs.push(['remove', r])
    })
  }
  if (npm.config.get('json')) {
    return this.printInstalledForJSON(diffs, cb)
  } else if (npm.config.get('parseable')) {
    return this.printInstalledForParseable(diffs, cb)
  } else {
    return this.printInstalledForHuman(diffs, cb)
  }
}

Installer.prototype.printInstalledForHuman = function (diffs, cb) {
  var removed = 0
  var added = 0
  var updated = 0
  var moved = 0
  // Count the number of contributors to packages added, tracking
  // contributors we've seen, so we can produce a running unique count.
  var contributors = new Set()
  diffs.forEach(function (action) {
    var mutation = action[0]
    var pkg = action[1]
    if (pkg.failed) return
    if (mutation === 'remove') {
      ++removed
    } else if (mutation === 'move') {
      ++moved
    } else if (mutation === 'add') {
      ++added
      // Count contributors to added packages. Start by combining `author`
      // and `contributors` data into a single array of contributor-people
      // for this package.
      var people = []
      var meta = pkg.package
      if (meta.author) people.push(meta.author)
      if (meta.contributors && Array.isArray(meta.contributors)) {
        people = people.concat(meta.contributors)
      }
      // Make sure a normalized string for every person behind this
      // package is in `contributors`.
      people.forEach(function (person) {
        // Ignore errors from malformed `author` and `contributors`.
        try {
          var normalized = normalizePerson(person)
        } catch (error) {
          return
        }
        if (!contributors.has(normalized)) contributors.add(normalized)
      })
    } else if (mutation === 'update' || mutation === 'update-linked') {
      ++updated
    }
  })
  var report = ''
  if (this.args.length && (added || updated)) {
    report += this.args.map((p) => {
      return `+ ${p.name}@${p.version}`
    }).join('\n') + '\n'
  }
  var actions = []
  if (added) {
    var action = 'added ' + packages(added)
    if (contributors.size) action += from(contributors.size)
    actions.push(action)
  }
  if (removed) actions.push('removed ' + packages(removed))
  if (updated) actions.push('updated ' + packages(updated))
  if (moved) actions.push('moved ' + packages(moved))
  if (actions.length === 0) {
    report += 'up to date'
  } else if (actions.length === 1) {
    report += actions[0]
  } else {
    var lastAction = actions.pop()
    report += actions.join(', ') + ' and ' + lastAction
  }
  report += ' in ' + ((Date.now() - this.started) / 1000) + 's'

  output(report)
  return cb()

  function packages (num) {
    return num + ' package' + (num > 1 ? 's' : '')
  }

  function from (num) {
    return ' from ' + num + ' contributor' + (num > 1 ? 's' : '')
  }

  // Values of `author` and elements of `contributors` in `package.json`
  // files can be e-mail style strings or Objects with `name`, `email,
  // and `url` String properties.  Convert Objects to Strings so that
  // we can efficiently keep a set of contributors we have already seen.
  function normalizePerson (argument) {
    if (typeof argument === 'string') return argument
    var returned = ''
    if (argument.name) returned += argument.name
    if (argument.email) returned += ' <' + argument.email + '>'
    if (argument.url) returned += ' (' + argument.email + ')'
    return returned
  }
}

Installer.prototype.printInstalledForJSON = function (diffs, cb) {
  var result = {
    added: [],
    removed: [],
    updated: [],
    moved: [],
    failed: [],
    warnings: [],
    elapsed: Date.now() - this.started
  }
  var self = this
  this.idealTree.warnings.forEach(function (warning) {
    if (warning.code === 'EPACKAGEJSON' && self.global) return
    if (warning.code === 'ENOTDIR') return
    var output = errorMessage(warning)
    var message = flattenMessage(output.summary)
    if (output.detail.length) {
      message += '\n' + flattenMessage(output.detail)
    }
    result.warnings.push(message)
  })
  diffs.forEach(function (action) {
    var mutation = action[0]
    var child = action[1]
    var record = recordAction(action)
    if (child.failed) {
      result.failed.push(record)
    } else if (mutation === 'add') {
      result.added.push(record)
    } else if (mutation === 'update' || mutation === 'update-linked') {
      result.updated.push(record)
    } else if (mutation === 'move') {
      result.moved.push(record)
    } else if (mutation === 'remove') {
      result.removed.push(record)
    }
  })
  output(JSON.stringify(result, null, 2))
  cb()

  function flattenMessage (msg) {
    return msg.map(function (logline) { return logline.slice(1).join(' ') }).join('\n')
  }

  function recordAction (action) {
    var mutation = action[0]
    var child = action[1]
    var result = {
      action: mutation,
      name: moduleName(child),
      version: child.package && child.package.version,
      path: child.path
    }
    if (mutation === 'move') {
      result.previousPath = child.fromPath
    } else if (mutation === 'update') {
      result.previousVersion = child.oldPkg.package && child.oldPkg.package.version
    }
    return result
  }
}

Installer.prototype.printInstalledForParseable = function (diffs, cb) {
  var self = this
  diffs.forEach(function (action) {
    var mutation = action[0]
    var child = action[1]
    if (mutation === 'move') {
      var previousPath = path.relative(self.where, child.fromPath)
    } else if (mutation === 'update') {
      var previousVersion = child.oldPkg.package && child.oldPkg.package.version
    }
    output(
      mutation + '\t' +
      moduleName(child) + '\t' +
      (child.package ? child.package.version : '') + '\t' +
      (child.path ? path.relative(self.where, child.path) : '') + '\t' +
      (previousVersion || '') + '\t' +
      (previousPath || ''))
  })
  return cb()
}

Installer.prototype.debugActions = function (name, actionListName, cb) {
  validate('SSF', arguments)
  var actionsToLog = this[actionListName]
  log.silly(name, 'action count', actionsToLog.length)
  actionsToLog.forEach(function (action) {
    log.silly(name, action.map(function (value) {
      return (value && value.package) ? packageId(value) : value
    }).join(' '))
  })
  cb()
}

// This takes an object and a property name instead of a value to allow us
// to define the arguments for use by chain before the property exists yet.
Installer.prototype.debugTree = function (name, treeName, cb) {
  validate('SSF', arguments)
  log.silly(name, this.archyDebugTree(this[treeName]).trim())
  cb()
}

Installer.prototype.archyDebugTree = function (tree) {
  validate('O', arguments)
  var seen = new Set()
  function byName (aa, bb) {
    return packageId(aa).localeCompare(packageId(bb))
  }
  function expandTree (tree) {
    seen.add(tree)
    return {
      label: packageId(tree),
      nodes: tree.children.filter((tree) => { return !seen.has(tree) && !tree.removed }).sort(byName).map(expandTree)
    }
  }
  return archy(expandTree(tree), '', { unicode: npm.config.get('unicode') })
}

Installer.prototype.debugLogicalTree = function (name, treeName, cb) {
  validate('SSF', arguments)
  this[treeName] && log.silly(name, this.archyDebugLogicalTree(this[treeName]).trim())
  cb()
}

Installer.prototype.archyDebugLogicalTree = function (tree) {
  validate('O', arguments)
  var seen = new Set()
  function byName (aa, bb) {
    return packageId(aa).localeCompare(packageId(bb))
  }
  function expandTree (tree) {
    seen.add(tree)
    return {
      label: packageId(tree),
      nodes: tree.requires.filter((tree) => { return !seen.has(tree) && !tree.removed }).sort(byName).map(expandTree)
    }
  }
  return archy(expandTree(tree), '', { unicode: npm.config.get('unicode') })
}
