'use strict'

const BB = require('bluebird')

const hookApi = require('libnpm/hook')
const npmConfig = require('./config/figgy-config.js')
const output = require('./utils/output.js')
const otplease = require('./utils/otplease.js')
const pudding = require('figgy-pudding')
const relativeDate = require('tiny-relative-date')
const Table = require('cli-table3')
const validate = require('aproba')
const npm = require('./npm')

hook.usage = [
  'npm hook add <pkg> <url> <secret> [--type=<type>]',
  'npm hook ls [pkg]',
  'npm hook rm <id>',
  'npm hook update <id> <url> <secret>'
].join('\n')

hook.completion = (opts, cb) => {
  validate('OF', [opts, cb])
  return cb(null, []) // fill in this array with completion values
}

const HookConfig = pudding({
  json: {},
  loglevel: {},
  parseable: {},
  silent: {},
  unicode: {}
})

function UsageError () {
  throw Object.assign(new Error(hook.usage), {code: 'EUSAGE'})
}

module.exports = (args, cb) => BB.try(() => hook(args)).then(
  val => cb(null, val),
  err => err.code === 'EUSAGE' ? cb(err.message) : cb(err)
)
function hook (args) {
  if (args.length === 4) { // secret is passed in the args
    // we have the user secret in the CLI args, we need to redact it from the referer.
    redactUserSecret()
  }
  return otplease(npmConfig(), opts => {
    opts = HookConfig(opts)
    switch (args[0]) {
      case 'add':
        return add(args[1], args[2], args[3], opts)
      case 'ls':
        return ls(args[1], opts)
      case 'rm':
        return rm(args[1], opts)
      case 'update':
      case 'up':
        return update(args[1], args[2], args[3], opts)
      default:
        UsageError()
    }
  })
}

function add (pkg, uri, secret, opts) {
  return hookApi.add(pkg, uri, secret, opts).then(hook => {
    if (opts.json) {
      output(JSON.stringify(hook, null, 2))
    } else if (opts.parseable) {
      output(Object.keys(hook).join('\t'))
      output(Object.keys(hook).map(k => hook[k]).join('\t'))
    } else if (!opts.silent && opts.loglevel !== 'silent') {
      output(`+ ${hookName(hook)} ${
        opts.unicode ? ' ➜ ' : ' -> '
      } ${hook.endpoint}`)
    }
  })
}

function ls (pkg, opts) {
  return hookApi.ls(opts.concat({package: pkg})).then(hooks => {
    if (opts.json) {
      output(JSON.stringify(hooks, null, 2))
    } else if (opts.parseable) {
      output(Object.keys(hooks[0]).join('\t'))
      hooks.forEach(hook => {
        output(Object.keys(hook).map(k => hook[k]).join('\t'))
      })
    } else if (!hooks.length) {
      output("You don't have any hooks configured yet.")
    } else if (!opts.silent && opts.loglevel !== 'silent') {
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

function rm (id, opts) {
  return hookApi.rm(id, opts).then(hook => {
    if (opts.json) {
      output(JSON.stringify(hook, null, 2))
    } else if (opts.parseable) {
      output(Object.keys(hook).join('\t'))
      output(Object.keys(hook).map(k => hook[k]).join('\t'))
    } else if (!opts.silent && opts.loglevel !== 'silent') {
      output(`- ${hookName(hook)} ${
        opts.unicode ? ' ✘ ' : ' X '
      } ${hook.endpoint}`)
    }
  })
}

function update (id, uri, secret, opts) {
  return hookApi.update(id, uri, secret, opts).then(hook => {
    if (opts.json) {
      output(JSON.stringify(hook, null, 2))
    } else if (opts.parseable) {
      output(Object.keys(hook).join('\t'))
      output(Object.keys(hook).map(k => hook[k]).join('\t'))
    } else if (!opts.silent && opts.loglevel !== 'silent') {
      output(`+ ${hookName(hook)} ${
        opts.unicode ? ' ➜ ' : ' -> '
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

function redactUserSecret () {
  const referer = npm.referer
  if (!referer) return
  const splittedReferer = referer.split(' ')
  splittedReferer[4] = '[REDACTED]'
  npm.referer = splittedReferer.join(' ')
}
