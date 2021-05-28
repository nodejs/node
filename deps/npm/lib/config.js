// don't expand so that we only assemble the set of defaults when needed
const configDefs = require('./utils/config/index.js')

const mkdirp = require('mkdirp-infer-owner')
const { dirname } = require('path')
const { promisify } = require('util')
const fs = require('fs')
const readFile = promisify(fs.readFile)
const writeFile = promisify(fs.writeFile)
const { spawn } = require('child_process')
const { EOL } = require('os')
const ini = require('ini')

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

const publicVar = k => !/^(\/\/[^:]+:)?_/.test(k)

const BaseCommand = require('./base-command.js')
class Config extends BaseCommand {
  static get description () {
    return 'Manage the npm configuration files'
  }

  /* istanbul ignore next - see test/lib/load-all-commands.js */
  static get name () {
    return 'config'
  }

  /* istanbul ignore next - see test/lib/load-all-commands.js */
  static get usage () {
    return [
      'set <key>=<value> [<key>=<value> ...]',
      'get [<key> [<key> ...]]',
      'delete <key> [<key> ...]',
      'list [--json]',
      'edit',
    ]
  }

  /* istanbul ignore next - see test/lib/load-all-commands.js */
  static get params () {
    return [
      'json',
      'global',
      'editor',
      'long',
    ]
  }

  async completion (opts) {
    const argv = opts.conf.argv.remain
    if (argv[1] !== 'config')
      argv.unshift('config')

    if (argv.length === 2) {
      const cmds = ['get', 'set', 'delete', 'ls', 'rm', 'edit']
      if (opts.partialWord !== 'l')
        cmds.push('list')

      return cmds
    }

    const action = argv[2]
    switch (action) {
      case 'set':
        // todo: complete with valid values, if possible.
        if (argv.length > 3)
          return []

        // fallthrough
        /* eslint no-fallthrough:0 */
      case 'get':
      case 'delete':
      case 'rm':
        return Object.keys(configDefs.definitions)
      case 'edit':
      case 'list':
      case 'ls':
      default:
        return []
    }
  }

  exec (args, cb) {
    this.config(args).then(() => cb()).catch(cb)
  }

  execWorkspaces (args, filters, cb) {
    this.npm.log.warn('config', 'This command does not support workspaces.')
    this.exec(args, cb)
  }

  async config ([action, ...args]) {
    this.npm.log.disableProgress()
    try {
      switch (action) {
        case 'set':
          await this.set(args)
          break
        case 'get':
          await this.get(args)
          break
        case 'delete':
        case 'rm':
        case 'del':
          await this.del(args)
          break
        case 'list':
        case 'ls':
          await (this.npm.config.get('json') ? this.listJson() : this.list())
          break
        case 'edit':
          await this.edit()
          break
        default:
          throw this.usageError()
      }
    } finally {
      this.npm.log.enableProgress()
    }
  }

  async set (args) {
    if (!args.length)
      throw this.usageError()

    const where = this.npm.config.get('global') ? 'global' : 'user'
    for (const [key, val] of Object.entries(keyValues(args))) {
      this.npm.log.info('config', 'set %j %j', key, val)
      this.npm.config.set(key, val || '', where)
      if (!this.npm.config.validate(where))
        this.npm.log.warn('config', 'omitting invalid config values')
    }

    await this.npm.config.save(where)
  }

  async get (keys) {
    if (!keys.length)
      return this.list()

    const out = []
    for (const key of keys) {
      if (!publicVar(key))
        throw `The ${key} option is protected, and cannot be retrieved in this way`

      const pref = keys.length > 1 ? `${key}=` : ''
      out.push(pref + this.npm.config.get(key))
    }
    this.npm.output(out.join('\n'))
  }

  async del (keys) {
    if (!keys.length)
      throw this.usageError()

    const where = this.npm.config.get('global') ? 'global' : 'user'
    for (const key of keys)
      this.npm.config.delete(key, where)
    await this.npm.config.save(where)
  }

  async edit () {
    const global = this.npm.config.get('global')
    const e = this.npm.config.get('editor')
    const where = global ? 'global' : 'user'
    const file = this.npm.config.data.get(where).source

    // save first, just to make sure it's synced up
    // this also removes all the comments from the last time we edited it.
    await this.npm.config.save(where)

    const data = (
      await readFile(file, 'utf8').catch(() => '')
    ).replace(/\r\n/g, '\n')
    const entries = Object.entries(configDefs.defaults)
    const defData = entries.reduce((str, [key, val]) => {
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

${data.split('\n').sort((a, b) => a.localeCompare(b, 'en')).join('\n').trim()}

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

  async list () {
    const msg = []
    const long = this.npm.config.get('long')
    for (const [where, { data, source }] of this.npm.config.data.entries()) {
      if (where === 'default' && !long)
        continue

      const keys = Object.keys(data).sort((a, b) => a.localeCompare(b, 'en'))
      if (!keys.length)
        continue

      msg.push(`; "${where}" config from ${source}`, '')
      for (const k of keys) {
        const v = publicVar(k) ? JSON.stringify(data[k]) : '(protected)'
        const src = this.npm.config.find(k)
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

    this.npm.output(msg.join('\n').trim())
  }

  async listJson () {
    const publicConf = {}
    for (const key in this.npm.config.list[0]) {
      if (!publicVar(key))
        continue

      publicConf[key] = this.npm.config.get(key)
    }
    this.npm.output(JSON.stringify(publicConf, null, 2))
  }
}

module.exports = Config
