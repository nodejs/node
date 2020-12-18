const npm = require('./npm.js')
const { defaults, types } = require('./utils/config.js')
const usageUtil = require('./utils/usage.js')
const output = require('./utils/output.js')

const mkdirp = require('mkdirp-infer-owner')
const { dirname } = require('path')
const { promisify } = require('util')
const fs = require('fs')
const readFile = promisify(fs.readFile)
const writeFile = promisify(fs.writeFile)
const { spawn } = require('child_process')
const { EOL } = require('os')
const ini = require('ini')

const usage = usageUtil(
  'config',
  'npm config set <key>=<value> [<key>=<value> ...]' +
  '\nnpm config get [<key> [<key> ...]]' +
  '\nnpm config delete <key> [<key> ...]' +
  '\nnpm config list [--json]' +
  '\nnpm config edit' +
  '\nnpm set <key>=<value> [<key>=<value> ...]' +
  '\nnpm get [<key> [<key> ...]]'
)

const cmd = (args, cb) => config(args).then(() => cb()).catch(cb)

const completion = (opts, cb) => {
  const argv = opts.conf.argv.remain
  if (argv[1] !== 'config')
    argv.unshift('config')

  if (argv.length === 2) {
    const cmds = ['get', 'set', 'delete', 'ls', 'rm', 'edit']
    if (opts.partialWord !== 'l')
      cmds.push('list')

    return cb(null, cmds)
  }

  const action = argv[2]
  switch (action) {
    case 'set':
      // todo: complete with valid values, if possible.
      if (argv.length > 3)
        return cb(null, [])

      // fallthrough
      /* eslint no-fallthrough:0 */
    case 'get':
    case 'delete':
    case 'rm':
      return cb(null, Object.keys(types))
    case 'edit':
    case 'list':
    case 'ls':
    default:
      return cb(null, [])
  }
}

const UsageError = () =>
  Object.assign(new Error(usage), { code: 'EUSAGE' })

const config = async ([action, ...args]) => {
  npm.log.disableProgress()
  try {
    switch (action) {
      case 'set':
        await set(args)
        break
      case 'get':
        await get(args)
        break
      case 'delete':
      case 'rm':
      case 'del':
        await del(args)
        break
      case 'list':
      case 'ls':
        await (npm.flatOptions.json ? listJson() : list())
        break
      case 'edit':
        await edit()
        break
      default:
        throw UsageError()
    }
  } finally {
    npm.log.enableProgress()
  }
}

// take an array of `[key, value, k2=v2, k3, v3, ...]` and turn into
// { key: value, k2: v2, k3: v3 }
const keyValues = args => {
  const kv = {}
  for (let i = 0; i < args.length; i++) {
    const arg = args[i].split('=')
    const key = arg.shift()
    const val = arg.length ? arg.join('=')
      : i < args.length - 1 ? args[++i]
      : ''
    kv[key.trim()] = val.trim()
  }
  return kv
}

const set = async (args) => {
  if (!args.length)
    throw UsageError()

  const where = npm.flatOptions.global ? 'global' : 'user'
  for (const [key, val] of Object.entries(keyValues(args))) {
    npm.log.info('config', 'set %j %j', key, val)
    npm.config.set(key, val || '', where)
    if (!npm.config.validate(where))
      npm.log.warn('config', 'omitting invalid config values')
  }

  await npm.config.save(where)
}

const get = async keys => {
  if (!keys.length)
    return list()

  const out = []
  for (const key of keys) {
    if (!publicVar(key))
      throw `The ${key} option is protected, and cannot be retrieved in this way`

    const pref = keys.length > 1 ? `${key}=` : ''
    out.push(pref + npm.config.get(key))
  }
  output(out.join('\n'))
}

const del = async keys => {
  if (!keys.length)
    throw UsageError()

  const where = npm.flatOptions.global ? 'global' : 'user'
  for (const key of keys)
    npm.config.delete(key, where)
  await npm.config.save(where)
}

const edit = async () => {
  const { editor: e, global } = npm.flatOptions
  const where = global ? 'global' : 'user'
  const file = npm.config.data.get(where).source

  // save first, just to make sure it's synced up
  // this also removes all the comments from the last time we edited it.
  await npm.config.save(where)

  const data = (
    await readFile(file, 'utf8').catch(() => '')
  ).replace(/\r\n/g, '\n')
  const defData = Object.entries(defaults).reduce((str, [key, val]) => {
    const obj = { [key]: val }
    const i = ini.stringify(obj)
      .replace(/\r\n/g, '\n') // normalizes output from ini.stringify
      .replace(/\n$/m, '')
      .replace(/^/g, '; ')
      .replace(/\n/g, '\n; ')
      .split('\n')
    return str + '\n' + i
  }, '')

  const tmpData = `;;;;
; npm ${where}config file: ${file}
; this is a simple ini-formatted file
; lines that start with semi-colons are comments
; run \`npm help 7 config\` for documentation of the various options
;
; Configs like \`@scope:registry\` map a scope to a given registry url.
;
; Configs like \`//<hostname>/:_authToken\` are auth that is restricted
; to the registry host specified.

${data.split('\n').sort((a, b) => a.localeCompare(b)).join('\n').trim()}

;;;;
; all available options shown below with default values
;;;;

${defData}
`.split('\n').join(EOL)
  await mkdirp(dirname(file))
  await writeFile(file, tmpData, 'utf8')
  await new Promise((resolve, reject) => {
    const [bin, ...args] = e.split(/\s+/)
    const editor = spawn(bin, [...args, file], { stdio: 'inherit' })
    editor.on('exit', (code) => {
      if (code)
        return reject(new Error(`editor process exited with code: ${code}`))
      return resolve()
    })
  })
}

const publicVar = k => !/^(\/\/[^:]+:)?_/.test(k)

const list = async () => {
  const msg = []
  const { long } = npm.flatOptions
  for (const [where, { data, source }] of npm.config.data.entries()) {
    if (where === 'default' && !long)
      continue

    const keys = Object.keys(data).sort((a, b) => a.localeCompare(b))
    if (!keys.length)
      continue

    msg.push(`; "${where}" config from ${source}`, '')
    for (const k of keys) {
      const v = publicVar(k) ? JSON.stringify(data[k]) : '(protected)'
      const src = npm.config.find(k)
      const overridden = src !== where
      msg.push((overridden ? '; ' : '') +
        `${k} = ${v} ${overridden ? `; overridden by ${src}` : ''}`)
    }
    msg.push('')
  }

  if (!long) {
    msg.push(
      `; node bin location = ${process.execPath}`,
      `; cwd = ${process.cwd()}`,
      `; HOME = ${process.env.HOME}`,
      '; Run `npm config ls -l` to show all defaults.'
    )
  }

  output(msg.join('\n').trim())
}

const listJson = async () => {
  const publicConf = {}
  for (const key in npm.config.list[0]) {
    if (!publicVar(key))
      continue

    publicConf[key] = npm.config.get(key)
  }
  output(JSON.stringify(publicConf, null, 2))
}

module.exports = Object.assign(cmd, { usage, completion })
