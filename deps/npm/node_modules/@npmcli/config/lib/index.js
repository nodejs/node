// TODO: set the scope config from package.json or explicit cli config
const walkUp = require('walk-up-path')
const ini = require('ini')
const nopt = require('nopt')
const mkdirp = require('mkdirp-infer-owner')
const mapWorkspaces = require('@npmcli/map-workspaces')
const rpj = require('read-package-json-fast')
const log = require('proc-log')

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
const _checkDeprecated = Symbol('checkDeprecated')
const _flatten = Symbol('flatten')
const _flatOptions = Symbol('flatOptions')

class Config {
  static get typeDefs () {
    return typeDefs
  }

  constructor ({
    definitions,
    shorthands,
    flatten,
    npmPath,

    // options just to override in tests, mostly
    env = process.env,
    argv = process.argv,
    platform = process.platform,
    execPath = process.execPath,
    cwd = process.cwd(),
  }) {
    // turn the definitions into nopt's weirdo syntax
    this.definitions = definitions
    const types = {}
    const defaults = {}
    this.deprecated = {}
    for (const [key, def] of Object.entries(definitions)) {
      defaults[key] = def.default
      types[key] = def.type
      if (def.deprecated) {
        this.deprecated[key] = def.deprecated.trim().replace(/\n +/, '\n')
      }
    }

    // populated the first time we flatten the object
    this[_flatOptions] = null
    this[_flatten] = flatten
    this.types = types
    this.shorthands = shorthands
    this.defaults = defaults

    this.npmPath = npmPath
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
    if (!this.loaded) {
      throw new Error('call config.load() before reading values')
    }
    return this[_find](key)
  }

  [_find] (key) {
    // have to look in reverse order
    const entries = [...this.data.entries()]
    for (let i = entries.length - 1; i > -1; i--) {
      const [where, { data }] = entries[i]
      if (hasOwnProperty(data, key)) {
        return where
      }
    }
    return null
  }

  get (key, where) {
    if (!this.loaded) {
      throw new Error('call config.load() before reading values')
    }
    return this[_get](key, where)
  }

  // we need to get values sometimes, so use this internal one to do so
  // while in the process of loading.
  [_get] (key, where = null) {
    if (where !== null && !confTypes.has(where)) {
      throw new Error('invalid config location param: ' + where)
    }
    const { data } = this.data.get(where || 'cli')
    return where === null || hasOwnProperty(data, key) ? data[key] : undefined
  }

  set (key, val, where = 'cli') {
    if (!this.loaded) {
      throw new Error('call config.load() before setting values')
    }
    if (!confTypes.has(where)) {
      throw new Error('invalid config location param: ' + where)
    }
    this[_checkDeprecated](key)
    const { data } = this.data.get(where)
    data[key] = val

    // this is now dirty, the next call to this.valid will have to check it
    this.data.get(where)[_valid] = null

    // the flat options are invalidated, regenerate next time they're needed
    this[_flatOptions] = null
  }

  get flat () {
    if (this[_flatOptions]) {
      return this[_flatOptions]
    }

    // create the object for flat options passed to deps
    process.emit('time', 'config:load:flatten')
    this[_flatOptions] = {}
    // walk from least priority to highest
    for (const { data } of this.data.values()) {
      this[_flatten](data, this[_flatOptions])
    }
    process.emit('timeEnd', 'config:load:flatten')

    return this[_flatOptions]
  }

  delete (key, where = 'cli') {
    if (!this.loaded) {
      throw new Error('call config.load() before deleting values')
    }
    if (!confTypes.has(where)) {
      throw new Error('invalid config location param: ' + where)
    }
    delete this.data.get(where).data[key]
  }

  async load () {
    if (this.loaded) {
      throw new Error('attempting to load npm config multiple times')
    }

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

    // warn if anything is not valid
    process.emit('time', 'config:load:validate')
    this.validate()
    process.emit('timeEnd', 'config:load:validate')

    // set this before calling setEnvs, so that we don't have to share
    // symbols, as that module also does a bunch of get operations
    this[_loaded] = true

    process.emit('time', 'config:load:credentials')
    const reg = this.get('registry')
    const creds = this.getCredentialsByURI(reg)
    // ignore this error because a failed set will strip out anything that
    // might be a security hazard, which was the intention.
    try {
      this.setCredentialsByURI(reg, creds)
    } catch (_) {}
    process.emit('timeEnd', 'config:load:credentials')

    // set proper globalPrefix now that everything is loaded
    this.globalPrefix = this.get('prefix')

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
    if (this.env.HOME) {
      return this.home = this.env.HOME
    }
    this.home = homedir()
  }

  loadGlobalPrefix () {
    if (this.globalPrefix) {
      throw new Error('cannot load default global prefix more than once')
    }

    if (this.env.PREFIX) {
      this.globalPrefix = this.env.PREFIX
    } else if (this.platform === 'win32') {
    // c:\node\node.exe --> prefix=c:\node\
      this.globalPrefix = dirname(this.execPath)
    } else {
      // /usr/local/bin/node --> prefix=/usr/local
      this.globalPrefix = dirname(dirname(this.execPath))

      // destdir only is respected on Unix
      if (this.env.DESTDIR) {
        this.globalPrefix = join(this.env.DESTDIR, this.globalPrefix)
      }
    }
  }

  loadEnv () {
    const conf = Object.create(null)
    for (const [envKey, envVal] of Object.entries(this.env)) {
      if (!/^npm_config_/i.test(envKey) || envVal === '') {
        continue
      }
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
    for (const [where, { valid }] of this.data.entries()) {
      if (valid === false || valid === null && !this.validate(where)) {
        return false
      }
    }
    return true
  }

  validate (where) {
    if (!where) {
      let valid = true
      for (const [where] of this.data.entries()) {
        // no need to validate our defaults, we know they're fine
        // cli was already validated when parsed the first time
        if (where === 'default' || where === 'builtin' || where === 'cli') {
          continue
        }
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

  // Returns true if the value is coming directly from the source defined
  // in default definitions, if the current value for the key config is
  // coming from any other different source, returns false
  isDefault (key) {
    const [defaultType, ...types] = [...confTypes]
    const defaultData = this.data.get(defaultType).data

    return hasOwnProperty(defaultData, key)
      && types.every(type => {
        const typeData = this.data.get(type).data
        return !hasOwnProperty(typeData, key)
      })
  }

  invalidHandler (k, val, type, source, where) {
    log.warn(
      'invalid config',
      k + '=' + JSON.stringify(val),
      `set in ${source}`
    )
    this.data.get(where)[_valid] = false

    if (Array.isArray(type)) {
      if (type.includes(typeDefs.url.type)) {
        type = typeDefs.url.type
      } else {
        /* istanbul ignore if - no actual configs matching this, but
         * path types SHOULD be handled this way, like URLs, for the
         * same reason */
        if (type.includes(typeDefs.path.type)) {
          type = typeDefs.path.type
        }
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
    log.warn('invalid config', msg, desc)
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
      if (er.code !== 'ENOENT') {
        log.verbose('config', `error loading ${where} config`, er)
      }
    } else {
      conf.raw = obj
      for (const [key, value] of Object.entries(obj)) {
        const k = envReplace(key, this.env)
        const v = this.parseField(value, k)
        if (where !== 'default') {
          this[_checkDeprecated](k, where, obj, [key, value])
        }
        conf.data[k] = v
      }
    }
  }

  [_checkDeprecated] (key, where, obj, kv) {
    // XXX a future npm version will make this a warning.
    // An even more future npm version will make this an error.
    if (this.deprecated[key]) {
      log.verbose('config', key, this.deprecated[key])
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
    // found by walking up the folder tree. either way, we load it before
    // we return to make sure localPrefix is set
    await this.loadLocalPrefix()

    if (this[_get]('global') === true || this[_get]('location') === 'global') {
      this.data.get('project').source = '(global mode enabled, ignored)'
      this.sources.set(this.data.get('project').source, 'project')
      return
    }

    const projectFile = resolve(this.localPrefix, '.npmrc')
    // if we're in the ~ directory, and there happens to be a node_modules
    // folder (which is not TOO uncommon, it turns out), then we can end
    // up loading the "project" config where the "userconfig" will be,
    // which causes some calamaties.  So, we only load project config if
    // it doesn't match what the userconfig will be.
    if (projectFile !== this[_get]('userconfig')) {
      return this[_loadFile](projectFile, 'project')
    } else {
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

    const cliWorkspaces = this[_get]('workspaces', 'cli')
    const isGlobal = this[_get]('global') || this[_get]('location') === 'global'

    for (const p of walkUp(this.cwd)) {
      const hasNodeModules = await stat(resolve(p, 'node_modules'))
        .then((st) => st.isDirectory())
        .catch(() => false)

      const hasPackageJson = await stat(resolve(p, 'package.json'))
        .then((st) => st.isFile())
        .catch(() => false)

      if (!this.localPrefix && (hasNodeModules || hasPackageJson)) {
        this.localPrefix = p

        // if workspaces are disabled, or we're in global mode, return now
        if (cliWorkspaces === false || isGlobal) {
          return
        }

        // otherwise, continue the loop
        continue
      }

      if (this.localPrefix && hasPackageJson) {
        // if we already set localPrefix but this dir has a package.json
        // then we need to see if `p` is a workspace root by reading its package.json
        // however, if reading it fails then we should just move on
        const pkg = await rpj(resolve(p, 'package.json')).catch(() => false)
        if (!pkg) {
          continue
        }

        const workspaces = await mapWorkspaces({ cwd: p, pkg })
        for (const w of workspaces.values()) {
          if (w === this.localPrefix) {
            // see if there's a .npmrc file in the workspace, if so log a warning
            const hasNpmrc = await stat(resolve(this.localPrefix, '.npmrc'))
              .then((st) => st.isFile())
              .catch(() => false)

            if (hasNpmrc) {
              log.warn(`ignoring workspace config at ${this.localPrefix}/.npmrc`)
            }

            // set the workspace in the default layer, which allows it to be overridden easily
            const { data } = this.data.get('default')
            data.workspace = [this.localPrefix]
            this.localPrefix = p
            log.info(`found workspace root at ${this.localPrefix}`)
            // we found a root, so we return now
            return
          }
        }
      }
    }

    if (!this.localPrefix) {
      this.localPrefix = this.cwd
    }
  }

  loadUserConfig () {
    return this[_loadFile](this[_get]('userconfig'), 'user')
  }

  loadGlobalConfig () {
    return this[_loadFile](this[_get]('globalconfig'), 'global')
  }

  async save (where) {
    if (!this.loaded) {
      throw new Error('call config.load() before saving')
    }
    if (!confFileTypes.has(where)) {
      throw new Error('invalid config location param: ' + where)
    }
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
      try {
        this.setCredentialsByURI(reg, creds)
      } catch (_) {}
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
      if (st && (st.uid !== myUid || st.gid !== myGid)) {
        await chown(conf.source, st.uid, st.gid).catch(() => {})
      }
    }
    const mode = where === 'user' ? 0o600 : 0o666
    await chmod(conf.source, mode)
  }

  clearCredentialsByURI (uri) {
    const nerfed = nerfDart(uri)
    const def = nerfDart(this.get('registry'))
    if (def === nerfed) {
      // do not delete email, that shouldn't be nerfed any more.
      // just delete the nerfed copy, if one exists.
      this.delete(`-authtoken`, 'user')
      this.delete(`_authToken`, 'user')
      this.delete(`_authtoken`, 'user')
      this.delete(`_auth`, 'user')
      this.delete(`_password`, 'user')
      this.delete(`username`, 'user')
    }
    this.delete(`${nerfed}:-authtoken`, 'user')
    this.delete(`${nerfed}:_authtoken`, 'user')
    this.delete(`${nerfed}:_authToken`, 'user')
    this.delete(`${nerfed}:_auth`, 'user')
    this.delete(`${nerfed}:_password`, 'user')
    this.delete(`${nerfed}:username`, 'user')
    this.delete(`${nerfed}:email`, 'user')
  }

  setCredentialsByURI (uri, { token, username, password, email }) {
    const nerfed = nerfDart(uri)
    const def = nerfDart(this.get('registry'))

    if (def === nerfed) {
      // remove old style auth info not limited to a single registry
      this.delete('_password', 'user')
      this.delete('username', 'user')
      this.delete('_auth', 'user')
      this.delete('_authtoken', 'user')
      this.delete('-authtoken', 'user')
      this.delete('_authToken', 'user')
    }

    // email used to be nerfed always.  if we're using the default
    // registry, de-nerf it.
    if (nerfed === def) {
      email = email ||
        this.get('email', 'user') ||
        this.get(`${nerfed}:email`, 'user')
      if (email) {
        this.set('email', email, 'user')
      }
    }

    // field that hasn't been used as documented for a LONG time,
    // and as of npm 7.10.0, isn't used at all.  We just always
    // send auth if we have it, only to the URIs under the nerf dart.
    this.delete(`${nerfed}:always-auth`, 'user')

    this.delete(`${nerfed}:-authtoken`, 'user')
    this.delete(`${nerfed}:_authtoken`, 'user')
    this.delete(`${nerfed}:email`, 'user')
    if (token) {
      this.set(`${nerfed}:_authToken`, token, 'user')
      this.delete(`${nerfed}:_password`, 'user')
      this.delete(`${nerfed}:username`, 'user')
    } else if (username || password) {
      if (!username) {
        throw new Error('must include username')
      }
      if (!password) {
        throw new Error('must include password')
      }
      this.delete(`${nerfed}:_authToken`, 'user')
      this.set(`${nerfed}:username`, username, 'user')
      // note: not encrypted, no idea why we bothered to do this, but oh well
      // protects against shoulder-hacks if password is memorable, I guess?
      const encoded = Buffer.from(password, 'utf8').toString('base64')
      this.set(`${nerfed}:_password`, encoded, 'user')
    } else {
      throw new Error('No credentials to set.')
    }
  }

  // this has to be a bit more complicated to support legacy data of all forms
  getCredentialsByURI (uri) {
    const nerfed = nerfDart(uri)
    const creds = {}

    const email = this.get(`${nerfed}:email`) || this.get('email')
    if (email) {
      creds.email = email
    }

    const tokenReg = this.get(`${nerfed}:_authToken`) ||
      this.get(`${nerfed}:_authtoken`) ||
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

    const authReg = this.get(`${nerfed}:_auth`)
    if (authReg) {
      const authDecode = Buffer.from(authReg, 'base64').toString('utf8')
      const authSplit = authDecode.split(':')
      creds.username = authSplit.shift()
      creds.password = authSplit.join(':')
      creds.auth = authReg
      return creds
    }

    // at this point, we can only use the values if the URI is the
    // default registry.
    const defaultNerf = nerfDart(this.get('registry'))
    if (nerfed !== defaultNerf) {
      return creds
    }

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
    if (!auth) {
      return creds
    }

    const authDecode = Buffer.from(auth, 'base64').toString('utf8')
    const authSplit = authDecode.split(':')
    creds.username = authSplit.shift()
    creds.password = authSplit.join(':')
    creds.auth = auth
    return creds
  }

  // set up the environment object we have with npm_config_* environs
  // for all configs that are different from their default values, and
  // set EDITOR and HOME.
  setEnvs () {
    setEnvs(this)
  }
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
    if (this[_source]) {
      throw new Error('cannot set ConfigData source more than once')
    }
    this[_source] = s
  }

  get source () {
    return this[_source]
  }

  set loadError (e) {
    if (this[_loadError] || this[_raw]) {
      throw new Error('cannot set ConfigData loadError after load')
    }
    this[_loadError] = e
  }

  get loadError () {
    return this[_loadError]
  }

  set raw (r) {
    if (this[_raw] || this[_loadError]) {
      throw new Error('cannot set ConfigData raw after load')
    }
    this[_raw] = r
  }

  get raw () {
    return this[_raw]
  }
}

module.exports = Config
