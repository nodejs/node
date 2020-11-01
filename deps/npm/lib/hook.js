const hookApi = require('libnpmhook')
const npm = require('./npm.js')
const output = require('./utils/output.js')
const otplease = require('./utils/otplease.js')
const relativeDate = require('tiny-relative-date')
const Table = require('cli-table3')

const usageUtil = require('./utils/usage.js')
const usage = usageUtil('hook', [
  'npm hook add <pkg> <url> <secret> [--type=<type>]',
  'npm hook ls [pkg]',
  'npm hook rm <id>',
  'npm hook update <id> <url> <secret>',
].join('\n'))

const completion = require('./utils/completion/none.js')

const cmd = (args, cb) => hook(args).then(() => cb()).catch(cb)

const hook = async (args) => otplease(npm.flatOptions, opts => {
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
      throw usage
  }
})

const add = (pkg, uri, secret, opts) => {
  hookApi.add(pkg, uri, secret, opts).then(hook => {
    if (opts.json)
      output(JSON.stringify(hook, null, 2))
    else if (opts.parseable) {
      output(Object.keys(hook).join('\t'))
      output(Object.keys(hook).map(k => hook[k]).join('\t'))
    } else if (!opts.silent && opts.loglevel !== 'silent') {
      output(`+ ${hookName(hook)} ${
        opts.unicode ? ' ➜ ' : ' -> '
      } ${hook.endpoint}`)
    }
  })
}

const ls = (pkg, opts) => {
  return hookApi.ls({ ...opts, package: pkg }).then(hooks => {
    if (opts.json)
      output(JSON.stringify(hooks, null, 2))
    else if (opts.parseable) {
      output(Object.keys(hooks[0]).join('\t'))
      hooks.forEach(hook => {
        output(Object.keys(hook).map(k => hook[k]).join('\t'))
      })
    } else if (!hooks.length)
      output("You don't have any hooks configured yet.")
    else if (!opts.silent && opts.loglevel !== 'silent') {
      if (hooks.length === 1)
        output('You have one hook configured.')
      else
        output(`You have ${hooks.length} hooks configured.`)

      const table = new Table({ head: ['id', 'target', 'endpoint'] })
      hooks.forEach((hook) => {
        table.push([
          { rowSpan: 2, content: hook.id },
          hookName(hook),
          hook.endpoint,
        ])
        if (hook.last_delivery) {
          table.push([
            {
              colSpan: 1,
              content: `triggered ${relativeDate(hook.last_delivery)}`,
            },
            hook.response_code,
          ])
        } else
          table.push([{ colSpan: 2, content: 'never triggered' }])
      })
      output(table.toString())
    }
  })
}

const rm = (id, opts) => {
  return hookApi.rm(id, opts).then(hook => {
    if (opts.json)
      output(JSON.stringify(hook, null, 2))
    else if (opts.parseable) {
      output(Object.keys(hook).join('\t'))
      output(Object.keys(hook).map(k => hook[k]).join('\t'))
    } else if (!opts.silent && opts.loglevel !== 'silent') {
      output(`- ${hookName(hook)} ${
        opts.unicode ? ' ✘ ' : ' X '
      } ${hook.endpoint}`)
    }
  })
}

const update = (id, uri, secret, opts) => {
  return hookApi.update(id, uri, secret, opts).then(hook => {
    if (opts.json)
      output(JSON.stringify(hook, null, 2))
    else if (opts.parseable) {
      output(Object.keys(hook).join('\t'))
      output(Object.keys(hook).map(k => hook[k]).join('\t'))
    } else if (!opts.silent && opts.loglevel !== 'silent') {
      output(`+ ${hookName(hook)} ${
        opts.unicode ? ' ➜ ' : ' -> '
      } ${hook.endpoint}`)
    }
  })
}

const hookName = (hook) => {
  let target = hook.name
  if (hook.type === 'scope')
    target = '@' + target
  if (hook.type === 'owner')
    target = '~' + target
  return target
}

module.exports = Object.assign(cmd, { usage, completion })
