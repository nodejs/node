'use strict'
// remove a package.

module.exports = uninstall

const path = require('path')
const validate = require('aproba')
const readJson = require('read-package-json')
const iferr = require('iferr')
const npm = require('./npm.js')
const Installer = require('./install.js').Installer
const getSaveType = require('./install/save.js').getSaveType
const removeDeps = require('./install/deps.js').removeDeps
const log = require('npmlog')
const usage = require('./utils/usage')

uninstall.usage = usage(
  'uninstall',
  'npm uninstall [<@scope>/]<pkg>[@<version>]... [--save-prod|--save-dev|--save-optional] [--no-save]'
)

uninstall.completion = require('./utils/completion/installed-shallow.js')

function uninstall (args, cb) {
  validate('AF', arguments)
  // the /path/to/node_modules/..
  const dryrun = !!npm.config.get('dry-run')

  if (args.length === 1 && args[0] === '.') args = []

  const where = npm.config.get('global') || !args.length
            ? path.resolve(npm.globalDir, '..')
            : npm.prefix

  args = args.filter(function (a) {
    return path.resolve(a) !== where
  })

  if (args.length) {
    new Uninstaller(where, dryrun, args).run(cb)
  } else {
    // remove this package from the global space, if it's installed there
    readJson(path.resolve(npm.localPrefix, 'package.json'), function (er, pkg) {
      if (er && er.code !== 'ENOENT' && er.code !== 'ENOTDIR') return cb(er)
      if (er) return cb(uninstall.usage)
      new Uninstaller(where, dryrun, [pkg.name]).run(cb)
    })
  }
}

class Uninstaller extends Installer {
  constructor (where, dryrun, args) {
    super(where, dryrun, args)
    this.remove = []
  }

  loadArgMetadata (next) {
    this.args = this.args.map(function (arg) { return {name: arg} })
    next()
  }

  loadAllDepsIntoIdealTree (cb) {
    validate('F', arguments)
    this.remove = this.args
    this.args = []
    log.silly('uninstall', 'loadAllDepsIntoIdealTree')
    const saveDeps = getSaveType()

    super.loadAllDepsIntoIdealTree(iferr(cb, () => {
      removeDeps(this.remove, this.idealTree, saveDeps, cb)
    }))
  }

  // no top level lifecycles on rm
  runPreinstallTopLevelLifecycles (cb) { cb() }
  runPostinstallTopLevelLifecycles (cb) { cb() }
}

module.exports.Uninstaller = Uninstaller
