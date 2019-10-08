'use strict'

const BB = require('bluebird')

const andAddParentToErrors = require('./and-add-parent-to-errors.js')
const failedDependency = require('./deps.js').failedDependency
const isInstallable = BB.promisify(require('./validate-args.js').isInstallable)
const moduleName = require('../utils/module-name.js')
const npm = require('../npm.js')
const reportOptionalFailure = require('./report-optional-failure.js')
const validate = require('aproba')

const actions = {}

actions.fetch = require('./action/fetch.js')
actions.extract = require('./action/extract.js')
actions.build = require('./action/build.js')
actions.preinstall = require('./action/preinstall.js')
actions.install = require('./action/install.js')
actions.postinstall = require('./action/postinstall.js')
actions.prepare = require('./action/prepare.js')
actions.finalize = require('./action/finalize.js')
actions.remove = require('./action/remove.js')
actions.unbuild = require('./action/unbuild.js')
actions.move = require('./action/move.js')
actions['global-install'] = require('./action/global-install.js')
actions['global-link'] = require('./action/global-link.js')
actions['refresh-package-json'] = require('./action/refresh-package-json.js')

// FIXME: We wrap actions like three ways to sunday here.
// Rewrite this to only work one way.

Object.keys(actions).forEach(function (actionName) {
  var action = actions[actionName]
  actions[actionName] = (staging, pkg, log) => {
    validate('SOO', [staging, pkg, log])
    // refuse to run actions for failed packages
    if (pkg.failed) return BB.resolve()
    if (action.rollback) {
      if (!pkg.rollback) pkg.rollback = []
      pkg.rollback.unshift(action.rollback)
    }
    if (action.commit) {
      if (!pkg.commit) pkg.commit = []
      pkg.commit.push(action.commit)
    }

    let actionP
    if (pkg.knownInstallable) {
      actionP = runAction(action, staging, pkg, log)
    } else {
      actionP = isInstallable(null, pkg.package).then(() => {
        pkg.knownInstallable = true
        return runAction(action, staging, pkg, log)
      })
    }

    return actionP.then(() => {
      log.finish()
    }, (err) => {
      return BB.fromNode((cb) => {
        andAddParentToErrors(pkg.parent, cb)(err)
      }).catch((err) => {
        return handleOptionalDepErrors(pkg, err)
      })
    })
  }
  actions[actionName].init = action.init || (() => BB.resolve())
  actions[actionName].teardown = action.teardown || (() => BB.resolve())
})
exports.actions = actions

function runAction (action, staging, pkg, log) {
  return BB.fromNode((cb) => {
    const result = action(staging, pkg, log, cb)
    if (result && result.then) {
      result.then(() => cb(), cb)
    }
  })
}

function markAsFailed (pkg) {
  if (pkg.failed) return
  pkg.failed = true
  pkg.requires.forEach((req) => {
    var requiredBy = req.requiredBy.filter((reqReqBy) => !reqReqBy.failed)
    if (requiredBy.length === 0 && !req.userRequired) {
      markAsFailed(req)
    }
  })
}

function handleOptionalDepErrors (pkg, err) {
  markAsFailed(pkg)
  var anyFatal = failedDependency(pkg)
  if (anyFatal) {
    throw err
  } else {
    reportOptionalFailure(pkg, null, err)
  }
}

exports.doOne = doOne
function doOne (cmd, staging, pkg, log, next) {
  validate('SSOOF', arguments)
  const prepped = prepareAction([cmd, pkg], staging, log)
  return withInit(actions[cmd], () => {
    return execAction(prepped)
  }).nodeify(next)
}

exports.doParallel = doParallel
function doParallel (type, staging, actionsToRun, log, next) {
  validate('SSAOF', arguments)
  const acts = actionsToRun.reduce((acc, todo) => {
    if (todo[0] === type) {
      acc.push(prepareAction(todo, staging, log))
    }
    return acc
  }, [])
  log.silly('doParallel', type + ' ' + acts.length)
  time(log)
  if (!acts.length) { return next() }
  return withInit(actions[type], () => {
    return BB.map(acts, execAction, {
      concurrency: npm.limit.action
    })
  }).nodeify((err) => {
    log.finish()
    timeEnd(log)
    next(err)
  })
}

exports.doSerial = doSerial
function doSerial (type, staging, actionsToRun, log, next) {
  validate('SSAOF', arguments)
  log.silly('doSerial', '%s %d', type, actionsToRun.length)
  runSerial(type, staging, actionsToRun, log, next)
}

exports.doReverseSerial = doReverseSerial
function doReverseSerial (type, staging, actionsToRun, log, next) {
  validate('SSAOF', arguments)
  log.silly('doReverseSerial', '%s %d', type, actionsToRun.length)
  runSerial(type, staging, [].concat(actionsToRun).reverse(), log, next)
}

function runSerial (type, staging, actionsToRun, log, next) {
  const acts = actionsToRun.reduce((acc, todo) => {
    if (todo[0] === type) {
      acc.push(prepareAction(todo, staging, log))
    }
    return acc
  }, [])
  time(log)
  if (!acts.length) { return next() }
  return withInit(actions[type], () => {
    return BB.each(acts, execAction)
  }).nodeify((err) => {
    log.finish()
    timeEnd(log)
    next(err)
  })
}

function time (log) {
  process.emit('time', 'action:' + log.name)
}
function timeEnd (log) {
  process.emit('timeEnd', 'action:' + log.name)
}

function withInit (action, body) {
  return BB.using(
    action.init().disposer(() => action.teardown()),
    body
  )
}

function prepareAction (action, staging, log) {
  validate('ASO', arguments)
  validate('SO', action)
  var cmd = action[0]
  var pkg = action[1]
  if (!actions[cmd]) throw new Error('Unknown decomposed command "' + cmd + '" (is it new?)')
  return [actions[cmd], staging, pkg, log.newGroup(cmd + ':' + moduleName(pkg))]
}

function execAction (todo) {
  return todo[0].apply(null, todo.slice(1))
}
