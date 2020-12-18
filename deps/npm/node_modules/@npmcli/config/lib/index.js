// TODO: set the scope config from package.json or explicit cli config
const walkUp = require('walk-up-path')
const ini = require('ini')
const nopt = require('nopt')
const mkdirp = require('mkdirp-infer-owner')

/* istanbul ignore next */
const myUid = process.getuid && process.getuid()
/* istanbul ignore next */
const myGid = process.getgid && process.getgid()

const { resolve, dirname, join } = require('path')
const { homedir } = require('os')
const { promisify } = require('util')
const fs = require('fs')
const readFile = promisify(fs.readFile)
const writeFile = promisify(fs.writeFile)
const chmod = promisify(fs.chmod)
const chown = promisify(fs.chown)
const unlink = promisify(fs.unlink)
const stat = promisify(fs.stat)

const hasOwnProperty = (obj, key) =>
  Object.prototype.hasOwnProperty.call(obj, key)

// define a custom getter, but turn into a normal prop
// if we set it.  otherwise it can't be set on child objects
const settableGetter = (obj, key, get) => {
  Object.defineProperty(obj, key, {
    get,
    set (value) {
      Object.defineProperty(obj, key, {
        value,
        configurable: true,
        writable: true,
        enumerable: true,
      })
    },
    configurable: true,
    enumerable: true,
  })
}

const typeDefs = require('./type-defs.js')
const nerfDart = require('./nerf-dart.js')
const envReplace = require('./env-replace.js')
const parseField = require('./parse-field.js')
const typeDescription = require('./type-description.js')
const setEnvs = require('./set-envs.js')
const getUserAgent = require('./get-user-agent.js')

// types that can be saved back to
const confFileTypes = new Set([
  'global',
  'user',
  'project',
])

const confTypes = new Set([
  'default',
  'builtin',
  ...confFileTypes,
  'env',
  'cli',
])

const _loaded = Symbol('loaded')
const _get = Symbol('get')
const _find = Symbol('find')
const _loadObject = Symbol('loadObject')
const _loadFile = Symbol('loadFile')

class Config {
  static get typeDefs () {
    return typeDefs
  }

  constructor ({
    types,
    shorthands,
    defaults,
    npmPath,

    // options just to override in tests, mostly
    env = process.env,
    argv = process.argv,
    log = require('./proc-log.js'),
    platform = process.platform,
    execPath = process.execPath,
    cwd = process.cwd(),
  }) {
    this.npmPath = npmPath
    this.types = types
    this.shorthands = shorthands
    this.defaults = defaults
    this.log = log
    this.argv = argv
    this.env = env
    this.execPath = execPath
    this.platform = platform
    this.cwd = cwd

    // set when we load configs
    this.globalPrefix = null
    this.localPrefix = null

    // defaults to env.HOME, but will always be *something*
    this.home = null

    // set up the prototype chain of config objects
    const wheres = [...confTypes]
    this.data = new Map()
    let parent = null
    for (const where of wheres) {
      this.data.set(where, parent = new ConfigData(parent))
    }

    this.data.set = () => {
      throw new Error('cannot change internal config data structure')
    }
    this.data.delete = () => {
      throw new Error('cannot change internal config data structure')
    }

    this.sources = new Map([])

    this.list = []
    for (const { data } of this.data.values()) {
      this.list.unshift(data)
    }
    Object.freeze(this.list)

    this[_loaded] = false
  }

  get loaded () {
    return this[_loaded]
  }

  get prefix () {
    return this[_get]('global') ? this.globalPrefix : this.localPrefix
  }

  // return the location where key is found.
  find (key) {
    if (!this.loaded)
      throw new Error('call config.load() before reading values')
    return this[_find](key)
  }
  [_find] (key) {
    // have to look in reverse order
    const entries = [...this.data.entries()]
    for (let i = entries.length - 1; i > -1; i--) {
      const [where, { data }] = entries[i]
      if (hasOwnProperty(data, key))
        return where
    }
    return null
  }

  get (key, where) {
    if (!this.loaded)
      throw new Error('call config.load() before reading values')
    return this[_get](key, where)
  }
  // we need to get values sometimes, so use this internal one to do so
  // while in the process of loading.
  [_get] (key, where = null) {
    if (where !== null && !confTypes.has(where)) {
      throw new Error('invalid config location param: ' + where)
    }
    const { data, source } = this.data.get(where || 'cli')
    return where === null || hasOwnProperty(data, key) ? data[key] : undefined
  }

  set (key, val, where = 'cli') {
    if (!this.loaded)
      throw new Error('call config.load() before setting values')
    if (!confTypes.has(where))
      throw new Error('invalid config location param: ' + where)
    if (key === '_auth') {
      const { email } = this.getCredentialsByURI(this.get('registry'))
      if (!email)
        throw new Error('Cannot set _auth without first setting email')
    }
    this.data.get(where).data[key] = val

    // this is now dirty, the next call to this.valid will have to check it
    this.data.get(where)[_valid] = null
  }

  delete (key, where = 'cli') {
    if (!this.loaded)
      throw new Error('call config.load() before deleting values')
    if (!confTypes.has(where))
      throw new Error('invalid config location param: ' + where)
    delete this.data.get(where).data[key]
  }

  async load () {
    if (this.loaded)
      throw new Error('attempting to load npm config multiple times')

    process.emit('time', 'config:load')
    // first load the defaults, which sets the global prefix
    process.emit('time', 'config:load:defaults')
    this.loadDefaults()
    process.emit('timeEnd', 'config:load:defaults')

    // next load the builtin config, as this sets new effective defaults
    process.emit('time', 'config:load:builtin')
    await this.loadBuiltinConfig()
    process.emit('timeEnd', 'config:load:builtin')

    // cli and env are not async, and can set the prefix, relevant to project
    process.emit('time', 'config:load:cli')
    this.loadCLI()
    process.emit('timeEnd', 'config:load:cli')
    process.emit('time', 'config:load:env')
    this.loadEnv()
    process.emit('timeEnd', 'config:load:env')

    // next project config, which can affect userconfig location
    process.emit('time', 'config:load:project')
    await this.loadProjectConfig()
    process.emit('timeEnd', 'config:load:project')
    // then user config, which can affect globalconfig location
    process.emit('time', 'config:load:user')
    await this.loadUserConfig()
    process.emit('timeEnd', 'config:load:user')
    // last but not least, global config file
    process.emit('time', 'config:load:global')
    await this.loadGlobalConfig()
    process.emit('timeEnd', 'config:load:global')

    // now the extras
    process.emit('time', 'config:load:cafile')
    await this.loadCAFile()
    process.emit('timeEnd', 'config:load:cafile')

    // warn if anything is not valid
    process.emit('time', 'config:load:validate')
    this.validate()
    process.emit('timeEnd', 'config:load:validate')

    // set this before calling setEnvs, so that we don't have to share
    // symbols, as that module also does a bunch of get operations
    this[_loaded] = true

    // set proper globalPrefix now that everything is loaded
    this.globalPrefix = this.get('prefix')

    process.emit('time', 'config:load:setUserAgent')
    this.setUserAgent()
    process.emit('timeEnd', 'config:load:setUserAgent')

    process.emit('time', 'config:load:setEnvs')
    this.setEnvs()
    process.emit('timeEnd', 'config:load:setEnvs')

    process.emit('timeEnd', 'config:load')
  }

  loadDefaults () {
    this.loadGlobalPrefix()
    this.loadHome()

    this[_loadObject]({
      ...this.defaults,
      prefix: this.globalPrefix,
    }, 'default', 'default values')

    const { data } = this.data.get('default')

    // the metrics-registry defaults to the current resolved value of
    // the registry, unless overridden somewhere else.
    settableGetter(data, 'metrics-registry', () => this[_get]('registry'))

    // if the prefix is set on cli, env, or userconfig, then we need to
    // default the globalconfig file to that location, instead of the default
    // global prefix.  It's weird that `npm get globalconfig --prefix=/foo`
    // returns `/foo/etc/npmrc`, but better to not change it at this point.
    settableGetter(data, 'globalconfig', () =>
      resolve(this[_get]('prefix'), 'etc/npmrc'))
  }

  loadHome () {
    if (this.env.HOME)
      return this.home = this.env.HOME
    this.home = homedir()
  }

  loadGlobalPrefix () {
    if (this.globalPrefix)
      throw new Error('cannot load default global prefix more than once')

    if (this.env.PREFIX) {
      this.globalPrefix = this.env.PREFIX
    } else if (this.platform === 'win32') {
    // c:\node\node.exe --> prefix=c:\node\
      this.globalPrefix = dirname(this.execPath)
    } else {
      // /usr/local/bin/node --> prefix=/usr/local
      this.globalPrefix = dirname(dirname(this.execPath))

      // destdir only is respected on Unix
      if (this.env.DESTDIR)
        this.globalPrefix = join(this.env.DESTDIR, this.globalPrefix)
    }
  }

  loadEnv () {
    const prefix = 'npm_config_'
    const conf = Object.create(null)
    for (const [envKey, envVal] of Object.entries(this.env)) {
      if (!/^npm_config_/i.test(envKey) || envVal === '')
        continue
      const key = envKey.substr('npm_config_'.length)
        .replace(/(?!^)_/g, '-') // don't replace _ at the start of the key
        .toLowerCase()
      conf[key] = envVal
    }
    this[_loadObject](conf, 'env', 'environment')
  }

  loadCLI () {
    nopt.invalidHandler = (k, val, type) =>
      this.invalidHandler(k, val, type, 'command line options', 'cli')
    const conf = nopt(this.types, this.shorthands, this.argv)
    nopt.invalidHandler = null
    this.parsedArgv = conf.argv
    delete conf.argv
    this[_loadObject](conf, 'cli', 'command line options')
  }

  get valid () {
    for (const [where, {valid}] of this.data.entries()) {
      if (valid === false || valid === null && !this.validate(where))
        return false
    }
    return true
  }

  validate (where) {
    if (!where) {
      let valid = true
      for (const [where, obj] of this.data.entries()) {
        // no need to validate our defaults, we know they're fine
        // cli was already validated when parsed the first time
        if (where === 'default' || where === 'builtin' || where === 'cli')
          continue
        const ret = this.validate(where)
        valid = valid && ret
      }
      return valid
    } else {
      const obj = this.data.get(where)
      obj[_valid] = true

      nopt.invalidHandler = (k, val, type) =>
        this.invalidHandler(k, val, type, obj.source, where)

      nopt.clean(obj.data, this.types, this.typeDefs)

      nopt.invalidHandler = null
      return obj[_valid]
    }
  }

  invalidHandler (k, val, type, source, where) {
    this.log.warn(
      'invalid config',
      k + '=' + JSON.stringify(val),
      `set in ${source}`
    )
    this.data.get(where)[_valid] = false

    if (Array.isArray(type)) {
      if (type.indexOf(typeDefs.url.type) !== -1)
        type = typeDefs.url.type
      else {
        /* istanbul ignore if - no actual configs matching this, but
         * path types SHOULD be handled this way, like URLs, for the
         * same reason */
        if (type.indexOf(typeDefs.path.type) !== -1)
          type = typeDefs.path.type
      }
    }

    const typeDesc = typeDescription(type)
    const oneOrMore = typeDesc.indexOf(Array) !== -1
    const mustBe = typeDesc
      .filter(m => m !== undefined && m !== Array)
    const oneOf = mustBe.length === 1 && oneOrMore ? ' one or more'
      : mustBe.length > 1 && oneOrMore ? ' one or more of:'
      : mustBe.length > 1 ? ' one of:'
      : ''
    const msg = 'Must be' + oneOf
    const desc = mustBe.length === 1 ? mustBe[0]
      : mustBe.filter(m => m !== Array)
        .map(n => typeof n === 'string' ? n : JSON.stringify(n))
        .join(', ')
    this.log.warn('invalid config', msg, desc)
  }

  [_loadObject] (obj, where, source, er = null) {
    const conf = this.data.get(where)
    if (conf.source) {
      const m = `double-loading "${where}" configs from ${source}, ` +
        `previously loaded from ${conf.source}`
      throw new Error(m)
    }

    if (this.sources.has(source)) {
      const m = `double-loading config "${source}" as "${where}", ` +
        `previously loaded as "${this.sources.get(source)}"`
      throw new Error(m)
    }

    conf.source = source
    this.sources.set(source, where)
    if (er) {
      conf.loadError = er
      if (er.code !== 'ENOENT')
        this.log.verbose('config', `error loading ${where} config`, er)
    } else {
      conf.raw = obj
      for (const [key, value] of Object.entries(obj)) {
        const k = envReplace(key, this.env)
        const v = this.parseField(value, k)
        conf.data[k] = v
      }
    }
  }

  // Parse a field, coercing it to the best type available.
  parseField (f, key, listElement = false) {
    return parseField(f, key, this, listElement)
  }

  async [_loadFile] (file, type) {
    process.emit('time', 'config:load:file:' + file)
    // only catch the error from readFile, not from the loadObject call
    await readFile(file, 'utf8').then(
      data => this[_loadObject](ini.parse(data), type, file),
      er => this[_loadObject](null, type, file, er)
    )
    process.emit('timeEnd', 'config:load:file:' + file)
  }

  loadBuiltinConfig () {
    return this[_loadFile](resolve(this.npmPath, 'npmrc'), 'builtin')
  }

  async loadProjectConfig () {
    // the localPrefix can be set by the CLI config, but otherwise is
    // found by walking up the folder tree
    await this.loadLocalPrefix()
    const projectFile = resolve(this.localPrefix, '.npmrc')
    // if we're in the ~ directory, and there happens to be a node_modules
    // folder (which is not TOO uncommon, it turns out), then we can end
    // up loading the "project" config where the "userconfig" will be,
    // which causes some calamaties.  So, we only load project config if
    // it doesn't match what the userconfig will be.
    if (projectFile !== this[_get]('userconfig'))
      return this[_loadFile](projectFile, 'project')
    else {
      this.data.get('project').source = '(same as "user" config, ignored)'
      this.sources.set(this.data.get('project').source, 'project')
    }
  }

  async loadLocalPrefix () {
    const cliPrefix = this[_get]('prefix', 'cli')
    if (cliPrefix) {
      this.localPrefix = cliPrefix
      return
    }

    for (const p of walkUp(this.cwd)) {
      // walk up until we have a nm dir or a pj file
      const hasAny = (await Promise.all([
        stat(resolve(p, 'node_modules'))
          .then(st => st.isDirectory())
          .catch(() => false),
        stat(resolve(p, 'package.json'))
          .then(st => st.isFile())
          .catch(() => false),
      ])).some(is => is)
      if (hasAny) {
        this.localPrefix = p
        return
      }
    }

    this.localPrefix = this.cwd
  }

  loadUserConfig () {
    return this[_loadFile](this[_get]('userconfig'), 'user')
  }

  loadGlobalConfig () {
    return this[_loadFile](this[_get]('globalconfig'), 'global')
  }

  async save (where) {
    if (!this.loaded)
      throw new Error('call config.load() before saving')
    if (!confFileTypes.has(where))
      throw new Error('invalid config location param: ' + where)
    const conf = this.data.get(where)
    conf[_raw] = { ...conf.data }
    conf[_loadError] = null

    // upgrade auth configs to more secure variants before saving
    if (where === 'user') {
      const reg = this.get('registry')
      const creds = this.getCredentialsByURI(reg)
      // we ignore this error because the failed set already removed
      // anything that might be a security hazard, and it won't be
      // saved back to the .npmrc file, so we're good.
      try { this.setCredentialsByURI(reg, creds) } catch (_) {}
    }

    const iniData = ini.stringify(conf.data).trim() + '\n'
    if (!iniData.trim()) {
      // ignore the unlink error (eg, if file doesn't exist)
      await unlink(conf.source).catch(er => {})
      return
    }
    const dir = dirname(conf.source)
    await mkdirp(dir)
    await writeFile(conf.source, iniData, 'utf8')
    // don't leave a root-owned config file lying around
    /* istanbul ignore if - this is best-effort and a pita to test */
    if (myUid === 0) {
      const st = await stat(dir).catch(() => null)
      if (st && (st.uid !== myUid || st.gid !== myGid))
        await chown(conf.source, st.uid, st.gid).catch(() => {})
    }
    const mode = where === 'user' ? 0o600 : 0o666
    await chmod(conf.source, mode)
  }

  clearCredentialsByURI (uri) {
    const nerfed = nerfDart(uri)
    const def = nerfDart(this.get('registry'))
    if (def === nerfed) {
      this.delete(`-authtoken`, 'user')
      this.delete(`_authToken`, 'user')
      this.delete(`_auth`, 'user')
      this.delete(`_password`, 'user')
      this.delete(`username`, 'user')
      this.delete(`email`, 'user')
    }
    this.delete(`${nerfed}:-authtoken`, 'user')
    this.delete(`${nerfed}:_authToken`, 'user')
    this.delete(`${nerfed}:_auth`, 'user')
    this.delete(`${nerfed}:_password`, 'user')
    this.delete(`${nerfed}:username`, 'user')
    this.delete(`${nerfed}:email`, 'user')
  }

  setCredentialsByURI (uri, { token, username, password, email, alwaysAuth }) {
    const nerfed = nerfDart(uri)
    const def = nerfDart(this.get('registry'))

    if (def === nerfed) {
      // remove old style auth info not limited to a single registry
      this.delete('_password', 'user')
      this.delete('username', 'user')
      this.delete('email', 'user')
      this.delete('_auth', 'user')
      this.delete('_authtoken', 'user')
      this.delete('_authToken', 'user')
    }

    this.delete(`${nerfed}:-authtoken`)
    if (token) {
      this.set(`${nerfed}:_authToken`, token, 'user')
      this.delete(`${nerfed}:_password`, 'user')
      this.delete(`${nerfed}:username`, 'user')
      this.delete(`${nerfed}:email`, 'user')
      this.delete(`${nerfed}:always-auth`, 'user')
    } else if (username || password || email) {
      if (username || password) {
        if (!username)
          throw new Error('must include username')
        if (!password)
          throw new Error('must include password')
      }
      if (!email)
        throw new Error('must include email')
      this.delete(`${nerfed}:_authToken`, 'user')
      if (username || password) {
        this.set(`${nerfed}:username`, username, 'user')
        // note: not encrypted, no idea why we bothered to do this, but oh well
        // protects against shoulder-hacks if password is memorable, I guess?
        const encoded = Buffer.from(password, 'utf8').toString('base64')
        this.set(`${nerfed}:_password`, encoded, 'user')
      }
      this.set(`${nerfed}:email`, email, 'user')
      if (alwaysAuth !== undefined)
        this.set(`${nerfed}:always-auth`, alwaysAuth, 'user')
      else
        this.delete(`${nerfed}:always-auth`, 'user')
    } else {
      throw new Error('No credentials to set.')
    }
  }

  // this has to be a bit more complicated to support legacy data of all forms
  getCredentialsByURI (uri) {
    const nerfed = nerfDart(uri)
    const creds = {}

    // you can set always-auth for a single registry, or as a default
    const alwaysAuthReg = this.get(`${nerfed}:always-auth`)
    if (alwaysAuthReg !== undefined)
      creds.alwaysAuth = !!alwaysAuthReg
    else
      creds.alwaysAuth = this.get('always-auth')

    const email = this.get(`${nerfed}:email`) || this.get('email')
    if (email)
      creds.email = email

    const tokenReg = this.get(`${nerfed}:_authToken`) ||
      this.get(`${nerfed}:-authtoken`) ||
      nerfed === nerfDart(this.get('registry')) && this.get('_authToken')

    if (tokenReg) {
      creds.token = tokenReg
      return creds
    }

    const userReg = this.get(`${nerfed}:username`)
    const passReg = this.get(`${nerfed}:_password`)
    if (userReg && passReg) {
      creds.username = userReg
      creds.password = Buffer.from(passReg, 'base64').toString('utf8')
      const auth = `${creds.username}:${creds.password}`
      creds.auth = Buffer.from(auth, 'utf8').toString('base64')
      return creds
    }

    // at this point, we can only use the values if the URI is the
    // default registry.
    const defaultNerf = nerfDart(this.get('registry'))
    if (nerfed !== defaultNerf)
      return creds

    const userDef = this.get('username')
    const passDef = this.get('_password')
    if (userDef && passDef) {
      creds.username = userDef
      creds.password = Buffer.from(passDef, 'base64').toString('utf8')
      const auth = `${creds.username}:${creds.password}`
      creds.auth = Buffer.from(auth, 'utf8').toString('base64')
      return creds
    }

    // Handle the old-style _auth=<base64> style for the default
    // registry, if set.
    const auth = this.get('_auth')
    if (!auth)
      return creds

    const authDecode = Buffer.from(auth, 'base64').toString('utf8')
    const authSplit = authDecode.split(':')
    creds.username = authSplit.shift()
    creds.password = authSplit.join(':')
    creds.auth = auth
    return creds
  }

  async loadCAFile () {
    const where = this[_find]('cafile')

    /* istanbul ignore if - it'll always be set in the defaults */
    if (!where)
      return

    const cafile = this[_get]('cafile', where)
    const ca = this[_get]('ca', where)

    // if you have a ca, or cafile is set to null, then nothing to do here.
    if (ca || !cafile)
      return

    const raw = await readFile(cafile, 'utf8').catch(er => {
      if (er.code !== 'ENOENT')
        throw er
    })
    if (!raw)
      return

    const delim = '-----END CERTIFICATE-----'
    const output = raw.replace(/\r\n/g, '\n').split(delim)
      .filter(section => section.trim())
      .map(section => section.trimLeft() + delim)

    // make it non-enumerable so we don't save it back by accident
    const { data } = this.data.get(where)
    Object.defineProperty(data, 'ca', {
      value: output,
      enumerable: false,
      configurable: true,
      writable: true,
    })
  }

  // the user-agent configuration is a template that gets populated
  // with some variables, that takes place here
  setUserAgent () {
    this.set('user-agent', getUserAgent(this))
  }

  // set up the environment object we have with npm_config_* environs
  // for all configs that are different from their default values, and
  // set EDITOR and HOME.
  setEnvs () { setEnvs(this) }
}

const _data = Symbol('data')
const _raw = Symbol('raw')
const _loadError = Symbol('loadError')
const _source = Symbol('source')
const _valid = Symbol('valid')
class ConfigData {
  constructor (parent) {
    this[_data] = Object.create(parent && parent.data)
    this[_source] = null
    this[_loadError] = null
    this[_raw] = null
    this[_valid] = true
  }

  get data () {
    return this[_data]
  }

  get valid () {
    return this[_valid]
  }

  set source (s) {
    if (this[_source])
      throw new Error('cannot set ConfigData source more than once')
    this[_source] = s
  }
  get source () { return this[_source] }

  set loadError (e) {
    if (this[_loadError] || this[_raw])
      throw new Error('cannot set ConfigData loadError after load')
    this[_loadError] = e
  }
  get loadError () { return this[_loadError] }

  set raw (r) {
    if (this[_raw] || this[_loadError])
      throw new Error('cannot set ConfigData raw after load')
    this[_raw] = r
  }
  get raw () { return this[_raw] }
}

module.exports = Config
