const npm = require('./npm.js')
const { defaults, types } = require('./utils/config.js')
const usageUtil = require('./utils/usage.js')
const output = require('./utils/output.js')

const editor = require('editor')
const mkdirp = require('mkdirp-infer-owner')
const { dirname } = require('path')
const { promisify } = require('util')
const fs = require('fs')
const readFile = promisify(fs.readFile)
const writeFile = promisify(fs.writeFile)
const { EOL } = require('os')
const ini = require('ini')

const usage = usageUtil(
  'config',
  'npm config set <key> <value>' +
  '\nnpm config get [<key>]' +
  '\nnpm config delete <key>' +
  '\nnpm config list [--json]' +
  '\nnpm config edit' +
  '\nnpm set <key> <value>' +
  '\nnpm get [<key>]'
)

const cmd = (args, cb) => config(args).then(() => cb()).catch(cb)

const completion = (opts, cb) => {
  const argv = opts.conf.argv.remain
  if (argv[1] !== 'config') argv.unshift('config')
  if (argv.length === 2) {
    const cmds = ['get', 'set', 'delete', 'ls', 'rm', 'edit']
    if (opts.partialWord !== 'l') cmds.push('list')
    return cb(null, cmds)
  }

  const action = argv[2]
  switch (action) {
    case 'set':
      // todo: complete with valid values, if possible.
      if (argv.length > 3) return cb(null, [])
      // fallthrough
      /* eslint no-fallthrough:0 */
    case 'get':
    case 'delete':
    case 'rm':
      return cb(null, Object.keys(types))
    case 'edit':
    case 'list':
    case 'ls':
      return cb(null, [])
    default:
      return cb(null, [])
  }
}

const config = async ([action, key, val]) => {
  npm.log.disableProgress()
  try {
    switch (action) {
      case 'set':
        await set(key, val)
        break
      case 'get':
        await get(key)
        break
      case 'delete':
      case 'rm':
      case 'del':
        await del(key)
        break
      case 'list':
      case 'ls':
        await (npm.config.get('json') ? listJson() : list())
        break
      case 'edit':
        await edit()
        break
      default:
        throw usage
    }
  } finally {
    npm.log.enableProgress()
  }
}

const set = async (key, val) => {
  if (key === undefined) {
    throw usage
  }

  if (val === undefined) {
    if (key.indexOf('=') !== -1) {
      const k = key.split('=')
      key = k.shift()
      val = k.join('=')
    } else {
      val = ''
    }
  }

  key = key.trim()
  val = val.trim()
  npm.log.info('config', 'set %j %j', key, val)
  const where = npm.config.get('global') ? 'global' : 'user'
  const validBefore = npm.config.data.get(where).valid
  console.error('validBefore?', validBefore)
  npm.config.set(key, val, where)
  if (!npm.config.validate(where)) {
    npm.log.warn('config', 'omitting invalid config values')
  }
  await npm.config.save(where)
}

const get = async key => {
  if (!key) {
    return list()
  }

  if (!publicVar(key)) {
    throw `The ${key} option is protected, and cannot be retrieved in this way`
  }

  output(npm.config.get(key))
}

const del = async key => {
  if (!key) {
    throw usage
  }

  const where = npm.config.get('global') ? 'global' : 'user'
  npm.config.delete(key, where)
  await npm.config.save(where)
}

const edit = async () => {
  const { editor: e, global } = npm.flatOptions
  if (!e) {
    throw new Error('No `editor` config or EDITOR envionment variable set')
  }

  const where = global ? 'global' : 'user'
  const file = npm.config.data.get(where).source

  // save first, just to make sure it's synced up
  // this also removes all the comments from the last time we edited it.
  await npm.config.save(where)
  const data = (await readFile(file, 'utf8').catch(() => '')).replace(/\r\n/g, '\n')
  const defData = Object.entries(defaults).reduce((str, [key, val]) => {
    const obj = { [key]: val }
    const i = ini.stringify(obj)
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
  await new Promise((res, rej) => {
    editor(file, { editor: e }, (er) => er ? rej(er) : res())
  })
}

const publicVar = k => !/^(\/\/[^:]+:)?_/.test(k)

const list = async () => {
  const msg = []
  const { long } = npm.flatOptions
  for (const [where, { data, source }] of npm.config.data.entries()) {
    if (where === 'default' && !long) {
      continue
    }
    const keys = Object.keys(data).sort((a, b) => a.localeCompare(b))
    if (!keys.length) {
      continue
    }
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
    if (!publicVar(key)) {
      continue
    }
    publicConf[key] = npm.config.get(key)
  }
  output(JSON.stringify(publicConf, null, 2))
}

module.exports = Object.assign(cmd, { usage, completion })
