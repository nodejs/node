'use strict'

const BB = require('bluebird')

const crypto = require('crypto')
const hookApi = require('libnpmhook')
const log = require('npmlog')
const npm = require('./npm.js')
const output = require('./utils/output.js')
const pudding = require('figgy-pudding')
const relativeDate = require('tiny-relative-date')
const Table = require('cli-table2')
const usage = require('./utils/usage.js')
const validate = require('aproba')

hook.usage = usage([
  'npm hook add <pkg> <url> <secret> [--type=<type>]',
  'npm hook ls [pkg]',
  'npm hook rm <id>',
  'npm hook update <id> <url> <secret>'
])

hook.completion = (opts, cb) => {
  validate('OF', [opts, cb])
  return cb(null, []) // fill in this array with completion values
}

const npmSession = crypto.randomBytes(8).toString('hex')
const hookConfig = pudding()
function config () {
  return hookConfig({
    refer: npm.refer,
    projectScope: npm.projectScope,
    log,
    npmSession
  }, npm.config)
}

module.exports = (args, cb) => BB.try(() => hook(args)).nodeify(cb)
function hook (args) {
  switch (args[0]) {
    case 'add':
      return add(args[1], args[2], args[3])
    case 'ls':
      return ls(args[1])
    case 'rm':
      return rm(args[1])
    case 'update':
    case 'up':
      return update(args[1], args[2], args[3])
  }
}

function add (pkg, uri, secret) {
  return hookApi.add(pkg, uri, secret, config())
    .then((hook) => {
      if (npm.config.get('json')) {
        output(JSON.stringify(hook, null, 2))
      } else {
        output(`+ ${hookName(hook)} ${
          npm.config.get('unicode') ? ' ➜ ' : ' -> '
        } ${hook.endpoint}`)
      }
    })
}

function ls (pkg) {
  return hookApi.ls(pkg, config())
    .then((hooks) => {
      if (npm.config.get('json')) {
        output(JSON.stringify(hooks, null, 2))
      } else if (!hooks.length) {
        output("You don't have any hooks configured yet.")
      } else {
        if (hooks.length === 1) {
          output('You have one hook configured.')
        } else {
          output(`You have ${hooks.length} hooks configured.`)
        }
        const table = new Table({head: ['id', 'target', 'endpoint']})
        hooks.forEach((hook) => {
          table.push([
            {rowSpan: 2, content: hook.id},
            hookName(hook),
            hook.endpoint
          ])
          if (hook.last_delivery) {
            table.push([
              {
                colSpan: 1,
                content: `triggered ${relativeDate(hook.last_delivery)}`
              },
              hook.response_code
            ])
          } else {
            table.push([{colSpan: 2, content: 'never triggered'}])
          }
        })
        output(table.toString())
      }
    })
}

function rm (id) {
  return hookApi.rm(id, config())
    .then((hook) => {
      if (npm.config.get('json')) {
        output(JSON.stringify(hook, null, 2))
      } else {
        output(`- ${hookName(hook)} ${
          npm.config.get('unicode') ? ' ✘ ' : ' X '
        } ${hook.endpoint}`)
      }
    })
}

function update (id, uri, secret) {
  return hookApi.update(id, uri, secret, config())
    .then((hook) => {
      if (npm.config.get('json')) {
        output(JSON.stringify(hook, null, 2))
      } else {
        output(`+ ${hookName(hook)} ${
          npm.config.get('unicode') ? ' ➜ ' : ' -> '
        } ${hook.endpoint}`)
      }
    })
}

function hookName (hook) {
  let target = hook.name
  if (hook.type === 'scope') { target = '@' + target }
  if (hook.type === 'owner') { target = '~' + target }
  return target
}
