// don't expand so that we only assemble the set of defaults when needed
const configDefs = require('../utils/config/index.js')

const { mkdir, readFile, writeFile } = require('fs/promises')
const { dirname, resolve } = require('path')
const { spawn } = require('child_process')
const { EOL } = require('os')
const ini = require('ini')
const localeCompare = require('@isaacs/string-locale-compare')('en')
const rpj = require('read-package-json-fast')
const log = require('../utils/log-shim.js')

// These are the configs that we can nerf-dart. Not all of them currently even
// *have* config definitions so we have to explicitly validate them here
const nerfDarts = [
  '_auth',
  '_authToken',
  'username',
  '_password',
  'email',
  'certfile',
  'keyfile',
]

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

const publicVar = k => {
  // _password
  if (k.startsWith('_')) {
    return false
  }
  // //localhost:8080/:_password
  if (k.startsWith('//') && k.includes(':_')) {
    return false
  }
  return true
}

const BaseCommand = require('../base-command.js')
class Config extends BaseCommand {
  static description = 'Manage the npm configuration files'
  static name = 'config'
  static usage = [
    'set <key>=<value> [<key>=<value> ...]',
    'get [<key> [<key> ...]]',
    'delete <key> [<key> ...]',
    'list [--json]',
    'edit',
    'fix',
  ]

  static params = [
    'json',
    'global',
    'editor',
    'location',
    'long',
  ]

  static ignoreImplicitWorkspace = false

  static skipConfigValidation = true

  async completion (opts) {
    const argv = opts.conf.argv.remain
    if (argv[1] !== 'config') {
      argv.unshift('config')
    }

    if (argv.length === 2) {
      const cmds = ['get', 'set', 'delete', 'ls', 'rm', 'edit', 'fix']
      if (opts.partialWord !== 'l') {
        cmds.push('list')
      }

      return cmds
    }

    const action = argv[2]
    switch (action) {
      case 'set':
        // todo: complete with valid values, if possible.
        if (argv.length > 3) {
          return []
        }

        // fallthrough
        /* eslint no-fallthrough:0 */
      case 'get':
      case 'delete':
      case 'rm':
        return Object.keys(configDefs.definitions)
      case 'edit':
      case 'list':
      case 'ls':
      case 'fix':
      default:
        return []
    }
  }

  async exec ([action, ...args]) {
    log.disableProgress()
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
          await (this.npm.flatOptions.json ? this.listJson() : this.list())
          break
        case 'edit':
          await this.edit()
          break
        case 'fix':
          await this.fix()
          break
        default:
          throw this.usageError()
      }
    } finally {
      log.enableProgress()
    }
  }

  async set (args) {
    if (!args.length) {
      throw this.usageError()
    }

    const where = this.npm.flatOptions.location
    for (const [key, val] of Object.entries(keyValues(args))) {
      log.info('config', 'set %j %j', key, val)
      const baseKey = key.split(':').pop()
      if (!this.npm.config.definitions[baseKey] && !nerfDarts.includes(baseKey)) {
        throw new Error(`\`${baseKey}\` is not a valid npm option`)
      }
      const deprecated = this.npm.config.definitions[baseKey]?.deprecated
      if (deprecated) {
        throw new Error(
          `The \`${baseKey}\` option is deprecated, and can not be set in this way${deprecated}`
        )
      }
      this.npm.config.set(key, val || '', where)
      if (!this.npm.config.validate(where)) {
        log.warn('config', 'omitting invalid config values')
      }
    }

    await this.npm.config.save(where)
  }

  async get (keys) {
    if (!keys.length) {
      return this.list()
    }

    const out = []
    for (const key of keys) {
      if (!publicVar(key)) {
        throw new Error(`The ${key} option is protected, and can not be retrieved in this way`)
      }

      const pref = keys.length > 1 ? `${key}=` : ''
      out.push(pref + this.npm.config.get(key))
    }
    this.npm.output(out.join('\n'))
  }

  async del (keys) {
    if (!keys.length) {
      throw this.usageError()
    }

    const where = this.npm.flatOptions.location
    for (const key of keys) {
      this.npm.config.delete(key, where)
    }
    await this.npm.config.save(where)
  }

  async edit () {
    const e = this.npm.flatOptions.editor
    const where = this.npm.flatOptions.location
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

${data.split('\n').sort(localeCompare).join('\n').trim()}

;;;;
; all available options shown below with default values
;;;;

${defData}
`.split('\n').join(EOL)
    await mkdir(dirname(file), { recursive: true })
    await writeFile(file, tmpData, 'utf8')
    await new Promise((res, rej) => {
      const [bin, ...args] = e.split(/\s+/)
      const editor = spawn(bin, [...args, file], { stdio: 'inherit' })
      editor.on('exit', (code) => {
        if (code) {
          return rej(new Error(`editor process exited with code: ${code}`))
        }
        return res()
      })
    })
  }

  async fix () {
    let problems

    try {
      this.npm.config.validate()
      return // if validate doesn't throw we have nothing to do
    } catch (err) {
      // coverage skipped because we don't need to test rethrowing errors
      // istanbul ignore next
      if (err.code !== 'ERR_INVALID_AUTH') {
        throw err
      }

      problems = err.problems
    }

    if (!this.npm.config.isDefault('location')) {
      problems = problems.filter((problem) => {
        return problem.where === this.npm.config.get('location')
      })
    }

    this.npm.config.repair(problems)
    const locations = []

    this.npm.output('The following configuration problems have been repaired:\n')
    const summary = problems.map(({ action, from, to, key, where }) => {
      // coverage disabled for else branch because it is intentionally omitted
      // istanbul ignore else
      if (action === 'rename') {
        // we keep track of which configs were modified here so we know what to save later
        locations.push(where)
        return `~ \`${from}\` renamed to \`${to}\` in ${where} config`
      } else if (action === 'delete') {
        locations.push(where)
        return `- \`${key}\` deleted from ${where} config`
      }
    }).join('\n')
    this.npm.output(summary)

    return await Promise.all(locations.map((location) => this.npm.config.save(location)))
  }

  async list () {
    const msg = []
    // long does not have a flattener
    const long = this.npm.config.get('long')
    for (const [where, { data, source }] of this.npm.config.data.entries()) {
      if (where === 'default' && !long) {
        continue
      }

      const keys = Object.keys(data).sort(localeCompare)
      if (!keys.length) {
        continue
      }

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
        `; node version = ${process.version}`,
        `; npm local prefix = ${this.npm.localPrefix}`,
        `; npm version = ${this.npm.version}`,
        `; cwd = ${process.cwd()}`,
        `; HOME = ${process.env.HOME}`,
        '; Run `npm config ls -l` to show all defaults.'
      )
      msg.push('')
    }

    if (!this.npm.global) {
      const pkgPath = resolve(this.npm.prefix, 'package.json')
      const pkg = await rpj(pkgPath).catch(() => ({}))

      if (pkg.publishConfig) {
        msg.push(`; "publishConfig" from ${pkgPath}`)
        msg.push('; This set of config values will be used at publish-time.', '')
        const pkgKeys = Object.keys(pkg.publishConfig).sort(localeCompare)
        for (const k of pkgKeys) {
          const v = publicVar(k) ? JSON.stringify(pkg.publishConfig[k]) : '(protected)'
          msg.push(`${k} = ${v}`)
        }
        msg.push('')
      }
    }

    this.npm.output(msg.join('\n').trim())
  }

  async listJson () {
    const publicConf = {}
    for (const key in this.npm.config.list[0]) {
      if (!publicVar(key)) {
        continue
      }

      publicConf[key] = this.npm.config.get(key)
    }
    this.npm.output(JSON.stringify(publicConf, null, 2))
  }
}

module.exports = Config
