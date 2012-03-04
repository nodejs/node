// Create a chain of config objects, in this priority order:
//
// CLI - the --foo things in the command line.
// ENV - all the things starting with npm_config_ in the environment
// USER - $HOME/.npmrc
// GLOBAL - $PREFIX/etc/npmrc
//
// If the CLI or ENV specify a userconfig, then that file is used
// as the USER config.
//
// If the CLI or ENV specify a globalconfig, then that file is used
// as the GLOBAL config.
//
// export npm_config_userconfig=/some/other/file
// export npm_config_globalconfig=global
//
// For implementation reasons, "_" in env vars is turned into "-". So,
// export npm_config_node_version

exports.resolveConfigs = resolveConfigs
exports.save = save
exports.saveConfig = saveConfig
exports.del = del
exports.get = get
exports.set = set
exports.unParseField = unParseField
exports.defaultConfig = null

Object.defineProperty(exports, "keys",
  { get : function () { return configList.keys }})

var fs = require("graceful-fs")
  , fstream = require("fstream")
  , rimraf = require("rimraf")
  , path = require("path")
  , nopt = require("nopt")
  , ini = require("ini")
  , ProtoList = require("proto-list")
  , mkdir = require("mkdirp")
  , npm = require("../npm.js")

  , log = require("./log.js")
  , configDefs = require("./config-defs.js")

  , myUid = process.env.SUDO_UID !== undefined
          ? process.env.SUDO_UID : (process.getuid && process.getuid())
  , myGid = process.env.SUDO_GID !== undefined
          ? process.env.SUDO_GID : (process.getgid && process.getgid())

  , eol = process.platform === "win32" ? "\r\n" : "\n"
  , privateKey = null
  , defaultConfig
  , configList = new ProtoList()
  , types = configDefs.types

  , TRANS = exports.TRANS =
    { "default" : 5
    , "builtin": 4
    , "global" : 3
    , "user" : 2
    , "env" : 1
    , "cli" : 0
    }

exports.configList = configList

// just put this here for a moment, so that the logs
// in the config-loading phase don't cause it to blow up.
configList.push({loglevel:"warn"})

function resolveConfigs (cli, cb_) {
  defaultConfig = defaultConfig || configDefs.defaults
  exports.defaultConfig = defaultConfig
  configList.pop()
  configList.push(defaultConfig)
  var cl = configList
    , dc = cl.pop()
  if (!cb_) cb_ = cli, cli = {}

  function cb (er) {
    //console.error("resolving configs")
    exports.resolved = true
    cb_(er)
  }

  cl.list.length = 0
  Object.keys(cli).forEach(function (k) {
    cli[k] = parseField(cli[k], k)
  })
  cl.push(cli)
  cl.push(parseEnv(process.env))

  parseFile(cl.get("userconfig") || dc.userconfig, function (er, conf) {
    if (er) return cb(er)
    cl.push(conf)

    // globalconfig and globalignorefile defaults
    // need to respond to the "prefix" setting up to this point.
    // Eg, `npm config get globalconfig --prefix ~/local` should
    // return `~/local/etc/npmrc`
    if (cl.get("prefix")) {
      dc.globalconfig = path.resolve(cl.get("prefix"), "etc", "npmrc")
      dc.globalignorefile = path.resolve(cl.get("prefix"), "etc", "npmignore")
    }

    parseFile( cl.get("globalconfig") || dc.globalconfig
             , function (er, conf) {
      if (er) return cb(er)

      if (conf.hasOwnProperty("prefix")) {
        log.warn("Cannot set prefix in globalconfig file"
                , cl.get("globalconfig"))
        delete conf.prefix
      }

      cl.push(conf)
      // the builtin config file, for distros to use.

      parseFile(path.resolve(__dirname, "../../npmrc"), function (er, conf) {
        if (er) conf = {}
        cl.push(conf)
        cl.push(dc)
        validate(cl)
        cb()
      })
    })
  })
}

function validate (cl) {
  // warn about invalid configs at every level.
  cl.list.forEach(function (conf, level) {
    // clean(data, types, typeDefs)
    nopt.clean(conf, configDefs.types)
  })
}


function parseEnv (env) {
  var conf = {}
  Object.keys(env)
    .filter(function (k) { return k.match(/^npm_config_[^_]/i) })
    .forEach(function (k) {
      if (!env[k]) return

      conf[k.replace(/^npm_config_/i, "")
            .toLowerCase()
            .replace(/_/g, "-")] = parseField(env[k], k)
    })
  return conf
}

function unParseField (f, k) {
  // type can be an array or single thing.
  var isPath = -1 !== [].concat(types[k]).indexOf(path)
  if (isPath) {
    if (typeof process.env.HOME !== 'undefined') {
      if (process.env.HOME.substr(-1) === "/") {
        process.env.HOME = process.env.HOME.slice(0, process.env.HOME.length-1)
      }
      if (f.indexOf(process.env.HOME) === 0) {
        f = "~"+f.substr(process.env.HOME.length)
      }
    }
  }
  return ini.safe(f)
}

function parseField (f, k, emptyIsFalse) {
  if (typeof f !== "string" && !(f instanceof String)) return f
  // type can be an array or single thing.
  var isPath = -1 !== [].concat(types[k]).indexOf(path)
    , isBool = -1 !== [].concat(types[k]).indexOf(Boolean)
    , isString = -1 !== [].concat(types[k]).indexOf(String)
  f = ini.unsafe((""+f).trim())
  if (isBool && !isString && f === "") return f = true
  switch (f) {
    case "true": return true
    case "false": return false
    case "null": return null
    case "undefined": return undefined
  }

  f = envReplace(f)

  if (isPath) {
    var homePattern = process.platform === "win32" ? /^~(\/|\\)/ : /^~\//
    if (f.match(homePattern) && process.env.HOME) {
      f = path.resolve(process.env.HOME, f.substr(2))
    }
    f = path.resolve(f)
  }

  return f
}

function parseFile (file, cb) {
  if (!file) return cb(null, {})
  log.verbose(file, "config file")
  fs.readFile(file, function (er, data) {
    // treat all errors as just an empty file
    if (er) return cb(null, {})
    var d = ini.parse(""+data)
      , f = {}
    Object.keys(d).forEach(function (k) {
      f[k] = parseField(d[k], k)
    })
    cb(null, parseAuth(f))
  })
}

function encryptAuth (config, cb) {
  if (config.username && config._password) {
    var b = new Buffer(config.username+":"+config._password)
    config._auth = b.toString("base64")
  }
  delete config.username
  delete config._password
  return cb(null, config)
}

function parseAuth (config) {
  //console.error("parsing config %j", config)
  if (!config._auth) return config
  var b = new Buffer(config._auth, "base64")
    , unpw = b.toString().split(":")
    , un = unpw.shift()
    , pw = unpw.join(":")
  config.username = un = (config.username || un)
  config._password = pw = (config._password || pw)
  b = new Buffer(un + ":" + pw)
  config._auth = b.toString("base64")
  return config
}

function save (which, cb) {
  if (typeof which === "function") cb = which, which = null
  if (!which) which = ["global", "user", "builtin"]
  if (!Array.isArray(which)) which = [which]
  var errState = null
    , done = which.length
    , failed = []
  which.forEach(function (c) {
    saveConfig(c, function (er) {
      if (errState) return
      if (er) return cb(errState = er)
      if (-- done === 0) return cb()
    })
  })
}

function saveConfig (which, file, cb) {
  if (typeof file === "function") cb = file, file = null
  if (!file) {
    switch (which) {
      case "builtin":
        file = path.resolve(__dirname, "../../npmrc")
        break
      case "global":
        file = configList.get("globalconfig")
        break
      default:
        file = configList.get("userconfig")
        which = "user"
    }
  }

  saveConfigfile
    ( file
    , configList.list[TRANS[which]]
    , which
    , cb )
}

function saveConfigfile (file, config, which, cb) {
  encryptAuth(config, function () { // ignore errors
    var data = {}
    Object.keys(config).forEach(function (k) {
      data[k] = unParseField(config[k], k)
    })
    data = ini.stringify(data)
    return (data.trim())
         ? writeConfigfile(file, data, which, cb)
         : rimraf(file, cb)
  })
}

function writeConfigfile (configfile, data, which, cb) {
  data = data.split(/\r*\n/).join(eol)
  var props = { type: "File", path: configfile }
  if (which === "user") {
    props.mode = 0600
    if (typeof myUid === "number") {
      props.uid = +myUid
      props.gid = +myGid
    }
  } else {
    props.mode = 0644
  }
  fstream.Writer(props)
    .on("close", cb)
    .on("error", cb)
    .end(data)
}

function snapshot (which) {
  var x = (!which) ? configList.snapshot
        : configList.list[TRANS[which]] ? configList.list[TRANS[which]]
        : undefined
  if (!x) return
  Object.keys(x).forEach(function (k) { if (k.match(/^_/)) delete x[k] })
  return x
}
function get (key, which) {
  return (!key) ? snapshot(which)
       : (!which) ? configList.get(key) // resolved
       : configList.list[TRANS[which]]
         ? envReplace(configList.list[TRANS[which]][key])
       : undefined
}

function envReplace (f) {
  if (typeof f !== "string" || !f) return f

  // replace any ${ENV} values with the appropriate environ.
  var envExpr = /(\\*)\$\{([^}]+)\}/g
  return f.replace(envExpr, function (orig, esc, name, i, s) {
    esc = esc.length && esc.length % 2
    if (esc) return orig
    if (undefined === process.env[name]) {
      throw new Error("Failed to replace env in config: "+orig)
    }
    return process.env[name]
  })
}

function del (key, which) {
  if (!which) configList.list.forEach(function (l) {
    delete l[key]
  })
  else if (configList.list[TRANS[which]]) {
    delete configList.list[TRANS[which]]
  }
}
function set (key, value, which) {
  which = which || "cli"
  if (configList.length === 1) {
    return new Error("trying to set before loading")
  }
  return configList.list[TRANS[which]][key] = value
}
