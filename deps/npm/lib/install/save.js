'use strict'

const deepSortObject = require('../utils/deep-sort-object.js')
const detectIndent = require('detect-indent')
const detectNewline = require('detect-newline')
const fs = require('graceful-fs')
const iferr = require('iferr')
const log = require('npmlog')
const moduleName = require('../utils/module-name.js')
const npm = require('../npm.js')
const parseJSON = require('../utils/parse-json.js')
const path = require('path')
const stringifyPackage = require('../utils/stringify-package')
const validate = require('aproba')
const without = require('lodash.without')
const writeFileAtomic = require('write-file-atomic')

// if the -S|--save option is specified, then write installed packages
// as dependencies to a package.json file.

exports.saveRequested = function (tree, andReturn) {
  validate('OF', arguments)
  savePackageJson(tree, andWarnErrors(andSaveShrinkwrap(tree, andReturn)))
}

function andSaveShrinkwrap (tree, andReturn) {
  validate('OF', arguments)
  return function (er) {
    validate('E', arguments)
    saveShrinkwrap(tree, andWarnErrors(andReturn))
  }
}

function andWarnErrors (cb) {
  validate('F', arguments)
  return function (er) {
    if (er) log.warn('saveError', er.message)
    arguments[0] = null
    cb.apply(null, arguments)
  }
}

exports.saveShrinkwrap = saveShrinkwrap

function saveShrinkwrap (tree, next) {
  validate('OF', arguments)
  if (!npm.config.get('shrinkwrap') || !npm.config.get('package-lock')) {
    return next()
  }
  require('../shrinkwrap.js').createShrinkwrap(tree, {silent: false}, next)
}

function savePackageJson (tree, next) {
  validate('OF', arguments)
  var saveBundle = npm.config.get('save-bundle')

  // each item in the tree is a top-level thing that should be saved
  // to the package.json file.
  // The relevant tree shape is { <folder>: {what:<pkg>} }
  var saveTarget = path.resolve(tree.path, 'package.json')
  // don't use readJson, because we don't want to do all the other
  // tricky npm-specific stuff that's in there.
  fs.readFile(saveTarget, 'utf8', iferr(next, function (packagejson) {
    const indent = detectIndent(packagejson).indent
    const newline = detectNewline(packagejson)
    try {
      tree.package = parseJSON(packagejson)
    } catch (ex) {
      return next(ex)
    }

    // If we're saving bundled deps, normalize the key before we start
    if (saveBundle) {
      var bundle = tree.package.bundleDependencies || tree.package.bundledDependencies
      delete tree.package.bundledDependencies
      if (!Array.isArray(bundle)) bundle = []
    }

    var toSave = getThingsToSave(tree)
    var toRemove = getThingsToRemove(tree)
    var savingTo = {}
    toSave.forEach(function (pkg) { if (pkg.save) savingTo[pkg.save] = true })
    toRemove.forEach(function (pkg) { if (pkg.save) savingTo[pkg.save] = true })

    Object.keys(savingTo).forEach(function (save) {
      if (!tree.package[save]) tree.package[save] = {}
    })

    log.verbose('saving', toSave)
    const types = ['dependencies', 'devDependencies', 'optionalDependencies']
    toSave.forEach(function (pkg) {
      if (pkg.save) tree.package[pkg.save][pkg.name] = pkg.spec
      const movedFrom = []
      for (let saveType of types) {
        if (
          pkg.save !== saveType &&
          tree.package[saveType] &&
          tree.package[saveType][pkg.name]
        ) {
          movedFrom.push(saveType)
          delete tree.package[saveType][pkg.name]
        }
      }
      if (movedFrom.length) {
        log.notice('save', `${pkg.name} is being moved from ${movedFrom.join(' and ')} to ${pkg.save}`)
      }
      if (saveBundle) {
        var ii = bundle.indexOf(pkg.name)
        if (ii === -1) bundle.push(pkg.name)
      }
    })

    toRemove.forEach(function (pkg) {
      if (pkg.save) delete tree.package[pkg.save][pkg.name]
      if (saveBundle) {
        bundle = without(bundle, pkg.name)
      }
    })

    Object.keys(savingTo).forEach(function (key) {
      tree.package[key] = deepSortObject(tree.package[key])
    })
    if (saveBundle) {
      tree.package.bundleDependencies = deepSortObject(bundle)
    }

    var json = stringifyPackage(tree.package, indent, newline)
    if (json === packagejson) {
      log.verbose('shrinkwrap', 'skipping write for package.json because there were no changes.')
      next()
    } else {
      writeFileAtomic(saveTarget, json, next)
    }
  }))
}

exports.getSaveType = function (tree, arg) {
  if (arguments.length) validate('OO', arguments)
  var globalInstall = npm.config.get('global')
  var noSaveFlags = !npm.config.get('save') &&
                    !npm.config.get('save-dev') &&
                    !npm.config.get('save-prod') &&
                    !npm.config.get('save-optional')
  if (globalInstall || noSaveFlags) return null

  if (npm.config.get('save-optional')) {
    return 'optionalDependencies'
  } else if (npm.config.get('save-dev')) {
    return 'devDependencies'
  } else if (npm.config.get('save-prod')) {
    return 'dependencies'
  } else {
    if (arg) {
      var name = moduleName(arg)
      if (tree.package.optionalDependencies[name]) {
        return 'optionalDependencies'
      } else if (tree.package.devDependencies[name]) {
        return 'devDependencies'
      }
    }
    return 'dependencies'
  }
}

function getThingsToSave (tree) {
  validate('O', arguments)
  var toSave = tree.children.filter(function (child) {
    return child.save
  }).map(function (child) {
    return {
      name: moduleName(child),
      spec: child.saveSpec,
      save: child.save
    }
  })
  return toSave
}

function getThingsToRemove (tree) {
  validate('O', arguments)
  if (!tree.removedChildren) return []
  var toRemove = tree.removedChildren.map(function (child) {
    return {
      name: moduleName(child),
      save: child.save
    }
  })
  return toRemove
}
