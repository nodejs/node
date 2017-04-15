var CC = require('config-chain').ConfigChain
var inherits = require('inherits')
var configDefs = require('./defaults.js')
var types = configDefs.types
var once = require('once')
var fs = require('fs')
var path = require('path')
var nopt = require('nopt')
var ini = require('ini')
var Umask = configDefs.Umask
var mkdirp = require('mkdirp')
var umask = require('../utils/umask')
var isWindows = require('../utils/is-windows.js')

exports.load = load
exports.Conf = Conf
exports.loaded = false
exports.rootConf = null
exports.usingBuiltin = false
exports.defs = configDefs

Object.defineProperty(exports, 'defaults', { get: function () {
  return configDefs.defaults
}, enumerable: true })

Object.defineProperty(exports, 'types', { get: function () {
  return configDefs.types
}, enumerable: true })

exports.validate = validate

var myUid = process.env.SUDO_UID !== undefined
          ? process.env.SUDO_UID : (process.getuid && process.getuid())
var myGid = process.env.SUDO_GID !== undefined
          ? process.env.SUDO_GID : (process.getgid && process.getgid())

var loading = false
var loadCbs = []
function load () {
  var cli, builtin, cb
  for (var i = 0; i < arguments.length; i++) {
    switch (typeof arguments[i]) {
      case 'string': builtin = arguments[i]; break
      case 'object': cli = arguments[i]; break
      case 'function': cb = arguments[i]; break
    }
  }

  if (!cb) cb = function () {}

  if (exports.loaded) {
    var ret = exports.loaded
    if (cli) {
      ret = new Conf(ret)
      ret.unshift(cli)
    }
    return process.nextTick(cb.bind(null, null, ret))
  }

  // either a fresh object, or a clone of the passed in obj
  if (!cli) {
    cli = {}
  } else {
    cli = Object.keys(cli).reduce(function (c, k) {
      c[k] = cli[k]
      return c
    }, {})
  }

  loadCbs.push(cb)
  if (loading) return

  loading = true

  cb = once(function (er, conf) {
    if (!er) {
      exports.loaded = conf
      loading = false
    }
    loadCbs.forEach(function (fn) {
      fn(er, conf)
    })
    loadCbs.length = 0
  })

  // check for a builtin if provided.
  exports.usingBuiltin = !!builtin
  var rc = exports.rootConf = new Conf()
  if (builtin) {
    rc.addFile(builtin, 'builtin')
  } else {
    rc.add({}, 'builtin')
  }

  rc.on('load', function () {
    load_(builtin, rc, cli, cb)
  })
  rc.on('error', cb)
}

function load_ (builtin, rc, cli, cb) {
  var defaults = configDefs.defaults
  var conf = new Conf(rc)

  conf.usingBuiltin = !!builtin
  conf.add(cli, 'cli')
  conf.addEnv()

  conf.loadPrefix(function (er) {
    if (er) return cb(er)

    // If you're doing `npm --userconfig=~/foo.npmrc` then you'd expect
    // that ~/.npmrc won't override the stuff in ~/foo.npmrc (or, indeed
    // be used at all).
    //
    // However, if the cwd is ~, then ~/.npmrc is the home for the project
    // config, and will override the userconfig.
    //
    // If you're not setting the userconfig explicitly, then it will be loaded
    // twice, which is harmless but excessive.  If you *are* setting the
    // userconfig explicitly then it will override your explicit intent, and
    // that IS harmful and unexpected.
    //
    // Solution: Do not load project config file that is the same as either
    // the default or resolved userconfig value.  npm will log a "verbose"
    // message about this when it happens, but it is a rare enough edge case
    // that we don't have to be super concerned about it.
    var projectConf = path.resolve(conf.localPrefix, '.npmrc')
    var defaultUserConfig = rc.get('userconfig')
    var resolvedUserConfig = conf.get('userconfig')
    if (!conf.get('global') &&
        projectConf !== defaultUserConfig &&
        projectConf !== resolvedUserConfig) {
      conf.addFile(projectConf, 'project')
      conf.once('load', afterPrefix)
    } else {
      conf.add({}, 'project')
      afterPrefix()
    }
  })

  function afterPrefix () {
    conf.addFile(conf.get('userconfig'), 'user')
    conf.once('error', cb)
    conf.once('load', afterUser)
  }

  function afterUser () {
    // globalconfig and globalignorefile defaults
    // need to respond to the 'prefix' setting up to this point.
    // Eg, `npm config get globalconfig --prefix ~/local` should
    // return `~/local/etc/npmrc`
    // annoying humans and their expectations!
    if (conf.get('prefix')) {
      var etc = path.resolve(conf.get('prefix'), 'etc')
      mkdirp(etc, function () {
        defaults.globalconfig = path.resolve(etc, 'npmrc')
        defaults.globalignorefile = path.resolve(etc, 'npmignore')
        afterUserContinuation()
      })
    } else {
      afterUserContinuation()
    }
  }

  function afterUserContinuation () {
    conf.addFile(conf.get('globalconfig'), 'global')

    // move the builtin into the conf stack now.
    conf.root = defaults
    conf.add(rc.shift(), 'builtin')
    conf.once('load', function () {
      conf.loadExtras(afterExtras)
    })
  }

  function afterExtras (er) {
    if (er) return cb(er)

    // warn about invalid bits.
    validate(conf)

    var cafile = conf.get('cafile')

    if (cafile) {
      return conf.loadCAFile(cafile, finalize)
    }

    finalize()
  }

  function finalize (er) {
    if (er) {
      return cb(er)
    }

    exports.loaded = conf
    cb(er, conf)
  }
}

// Basically the same as CC, but:
// 1. Always ini
// 2. Parses environment variable names in field values
// 3. Field values that start with ~/ are replaced with process.env.HOME
// 4. Can inherit from another Conf object, using it as the base.
inherits(Conf, CC)
function Conf (base) {
  if (!(this instanceof Conf)) return new Conf(base)

  CC.apply(this)

  if (base) {
    if (base instanceof Conf) {
      this.root = base.list[0] || base.root
    } else {
      this.root = base
    }
  } else {
    this.root = configDefs.defaults
  }
}

Conf.prototype.loadPrefix = require('./load-prefix.js')
Conf.prototype.loadCAFile = require('./load-cafile.js')
Conf.prototype.loadUid = require('./load-uid.js')
Conf.prototype.setUser = require('./set-user.js')
Conf.prototype.findPrefix = require('./find-prefix.js')
Conf.prototype.getCredentialsByURI = require('./get-credentials-by-uri.js')
Conf.prototype.setCredentialsByURI = require('./set-credentials-by-uri.js')
Conf.prototype.clearCredentialsByURI = require('./clear-credentials-by-uri.js')

Conf.prototype.loadExtras = function (cb) {
  this.setUser(function (er) {
    if (er) return cb(er)
    this.loadUid(function (er) {
      if (er) return cb(er)
      // Without prefix, nothing will ever work
      mkdirp(this.prefix, cb)
    }.bind(this))
  }.bind(this))
}

Conf.prototype.save = function (where, cb) {
  var target = this.sources[where]
  if (!target || !(target.path || target.source) || !target.data) {
    var er
    if (where !== 'builtin') er = new Error('bad save target: ' + where)
    if (cb) {
      process.nextTick(cb.bind(null, er))
      return this
    }
    return this.emit('error', er)
  }

  if (target.source) {
    var pref = target.prefix || ''
    Object.keys(target.data).forEach(function (k) {
      target.source[pref + k] = target.data[k]
    })
    if (cb) process.nextTick(cb)
    return this
  }

  var data = ini.stringify(target.data)

  var then = function then (er) {
    if (er) return done(er)

    fs.chmod(target.path, mode, done)
  }

  var done = function done (er) {
    if (er) {
      if (cb) return cb(er)
      else return this.emit('error', er)
    }
    this._saving --
    if (this._saving === 0) {
      if (cb) cb()
      this.emit('save')
    }
  }

  then = then.bind(this)
  done = done.bind(this)
  this._saving ++

  var mode = where === 'user' ? '0600' : '0666'
  if (!data.trim()) {
    fs.unlink(target.path, function () {
      // ignore the possible error (e.g. the file doesn't exist)
      done(null)
    })
  } else {
    mkdirp(path.dirname(target.path), function (er) {
      if (er) return then(er)
      fs.writeFile(target.path, data, 'utf8', function (er) {
        if (er) return then(er)
        if (where === 'user' && myUid && myGid) {
          fs.chown(target.path, +myUid, +myGid, then)
        } else {
          then()
        }
      })
    })
  }

  return this
}

Conf.prototype.addFile = function (file, name) {
  name = name || file
  var marker = { __source__: name }
  this.sources[name] = { path: file, type: 'ini' }
  this.push(marker)
  this._await()
  fs.readFile(file, 'utf8', function (er, data) {
    // just ignore missing files.
    if (er) return this.add({}, marker)

    this.addString(data, file, 'ini', marker)
  }.bind(this))
  return this
}

// always ini files.
Conf.prototype.parse = function (content, file) {
  return CC.prototype.parse.call(this, content, file, 'ini')
}

Conf.prototype.add = function (data, marker) {
  try {
    Object.keys(data).forEach(function (k) {
      data[k] = parseField(data[k], k)
    })
  } catch (e) {
    this.emit('error', e)
    return this
  }
  return CC.prototype.add.call(this, data, marker)
}

Conf.prototype.addEnv = function (env) {
  env = env || process.env
  var conf = {}
  Object.keys(env)
    .filter(function (k) { return k.match(/^npm_config_/i) })
    .forEach(function (k) {
      if (!env[k]) return

      // leave first char untouched, even if
      // it is a '_' - convert all other to '-'
      var p = k.toLowerCase()
               .replace(/^npm_config_/, '')
               .replace(/(?!^)_/g, '-')
      conf[p] = env[k]
    })
  return CC.prototype.addEnv.call(this, '', conf, 'env')
}

function parseField (f, k) {
  if (typeof f !== 'string' && !(f instanceof String)) return f

  // type can be an array or single thing.
  var typeList = [].concat(types[k])
  var isPath = typeList.indexOf(path) !== -1
  var isBool = typeList.indexOf(Boolean) !== -1
  var isString = typeList.indexOf(String) !== -1
  var isUmask = typeList.indexOf(Umask) !== -1
  var isNumber = typeList.indexOf(Number) !== -1

  f = ('' + f).trim()

  if (f.match(/^".*"$/)) {
    try {
      f = JSON.parse(f)
    } catch (e) {
      throw new Error('Failed parsing JSON config key ' + k + ': ' + f)
    }
  }

  if (isBool && !isString && f === '') return true

  switch (f) {
    case 'true': return true
    case 'false': return false
    case 'null': return null
    case 'undefined': return undefined
  }

  f = envReplace(f)

  if (isPath) {
    var homePattern = isWindows ? /^~(\/|\\)/ : /^~\//
    if (f.match(homePattern) && process.env.HOME) {
      f = path.resolve(process.env.HOME, f.substr(2))
    }
    f = path.resolve(f)
  }

  if (isUmask) f = umask.fromString(f)

  if (isNumber && !isNaN(f)) f = +f

  return f
}

function envReplace (f) {
  if (typeof f !== 'string' || !f) return f

  // replace any ${ENV} values with the appropriate environ.
  var envExpr = /(\\*)\$\{([^}]+)\}/g
  return f.replace(envExpr, function (orig, esc, name) {
    esc = esc.length && esc.length % 2
    if (esc) return orig
    if (undefined === process.env[name]) {
      throw new Error('Failed to replace env in config: ' + orig)
    }

    return process.env[name]
  })
}

function validate (cl) {
  // warn about invalid configs at every level.
  cl.list.forEach(function (conf) {
    nopt.clean(conf, configDefs.types)
  })

  nopt.clean(cl.root, configDefs.types)
}
