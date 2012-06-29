;(function(){
// windows: running "npm blah" in this folder will invoke WSH, not node.
if (typeof WScript !== "undefined") {
  WScript.echo("npm does not work when run\n"
              +"with the Windows Scripting Host\n\n"
              +"'cd' to a different directory,\n"
              +"or type 'npm.cmd <args>',\n"
              +"or type 'node npm <args>'.")
  WScript.quit(1)
  return
}


// FIXME there really ought to be a path.split in node core
require("path").SPLIT_CHAR = process.platform === "win32" ? "\\" : "/"

var EventEmitter = require("events").EventEmitter
  , npm = module.exports = new EventEmitter
  , config = require("./config.js")
  , ini = require("./utils/ini.js")
  , log = require("npmlog")
  , fs = require("graceful-fs")
  , path = require("path")
  , abbrev = require("abbrev")
  , which = require("which")
  , semver = require("semver")
  , findPrefix = require("./utils/find-prefix.js")
  , getUid = require("uid-number")
  , mkdirp = require("mkdirp")
  , slide = require("slide")
  , chain = slide.chain
  , RegClient = require("npm-registry-client")

// /usr/local is often a read-only fs, which is not
// well handled by node or mkdirp.  Just double-check
// in the case of errors when making the prefix dirs.
function mkdir (p, cb) {
  mkdirp(p, function (er, made) {
    // it could be that we couldn't create it, because it
    // already exists, and is on a read-only fs.
    if (er) {
      return fs.stat(p, function (er2, st) {
        if (er2 || !st.isDirectory()) return cb(er)
        return cb(null, made)
      })
    }
    return cb(er, made)
  })
}

npm.commands = {}

try {
  // startup, ok to do this synchronously
  var j = JSON.parse(fs.readFileSync(
    path.join(__dirname, "../package.json"))+"")
  npm.version = j.version
  npm.nodeVersionRequired = j.engines.node
  if (!semver.satisfies(process.version, j.engines.node)) {
    log.warn("unsupported version", [""
            ,"npm requires node version: "+j.engines.node
            ,"And you have: "+process.version
            ,"which is not satisfactory."
            ,""
            ,"Bad things will likely happen.  You have been warned."
            ,""].join("\n"))
  }
} catch (ex) {
  try {
    log.info("error reading version", ex)
  } catch (er) {}
  npm.version = ex
}

var commandCache = {}
  // short names for common things
  , aliases = { "rm" : "uninstall"
              , "r" : "uninstall"
              , "un" : "uninstall"
              , "unlink" : "uninstall"
              , "remove" : "uninstall"
              , "rb" : "rebuild"
              , "list" : "ls"
              , "la" : "ls"
              , "ll" : "ls"
              , "ln" : "link"
              , "i" : "install"
              , "up" : "update"
              , "c" : "config"
              , "info" : "view"
              , "show" : "view"
              , "find" : "search"
              , "s" : "search"
              , "se" : "search"
              , "author" : "owner"
              , "home" : "docs"
              , "unstar": "star" // same function
              , "apihelp" : "help"
              , "login": "adduser"
              , "add-user": "adduser"
              , "tst": "test"
              }

  , aliasNames = Object.keys(aliases)
  // these are filenames in .
  , cmdList = [ "install"
              , "uninstall"
              , "cache"
              , "config"
              , "set"
              , "get"
              , "update"
              , "outdated"
              , "prune"
              , "submodule"
              , "pack"

              , "rebuild"
              , "link"

              , "publish"
              , "star"
              , "tag"
              , "adduser"
              , "unpublish"
              , "owner"
              , "deprecate"
              , "shrinkwrap"

              , "help"
              , "help-search"
              , "ls"
              , "search"
              , "view"
              , "init"
              , "version"
              , "edit"
              , "explore"
              , "docs"
              , "bugs"
              , "faq"
              , "root"
              , "prefix"
              , "bin"
              , "whoami"

              , "test"
              , "stop"
              , "start"
              , "restart"
              , "run-script"
              , "completion"
              ]
  , plumbing = [ "build"
               , "unbuild"
               , "xmas"
               , "substack"
               ]
  , fullList = npm.fullList = cmdList.concat(aliasNames).filter(function (c) {
      return plumbing.indexOf(c) === -1
    })
  , abbrevs = abbrev(fullList)

Object.keys(abbrevs).concat(plumbing).forEach(function addCommand (c) {
  Object.defineProperty(npm.commands, c, { get : function () {
    if (!loaded) throw new Error(
      "Call npm.load(conf, cb) before using this command.\n"+
      "See the README.md or cli.js for example usage.")
    var a = npm.deref(c)
    if (c === "la" || c === "ll") {
      npm.config.set("long", true)
    }
    npm.command = c
    if (commandCache[a]) return commandCache[a]
    var cmd = require(__dirname+"/"+a+".js")
    commandCache[a] = function () {
      var args = Array.prototype.slice.call(arguments, 0)
      if (typeof args[args.length - 1] !== "function") {
        args.push(defaultCb)
      }
      if (args.length === 1) args.unshift([])
      cmd.apply(npm, args)
    }
    Object.keys(cmd).forEach(function (k) {
      commandCache[a][k] = cmd[k]
    })
    return commandCache[a]
  }, enumerable: fullList.indexOf(c) !== -1 })

  // make css-case commands callable via camelCase as well
  if (c.match(/\-([a-z])/)) {
    addCommand(c.replace(/\-([a-z])/g, function (a, b) {
      return b.toUpperCase()
    }))
  }
})

function defaultCb (er, data) {
  if (er) console.error(er.stack || er.message)
  else console.log(data)
}

npm.deref = function (c) {
  if (!c) return ""
  if (c.match(/[A-Z]/)) c = c.replace(/([A-Z])/g, function (m) {
    return "-" + m.toLowerCase()
  })
  if (plumbing.indexOf(c) !== -1) return c
  var a = abbrevs[c]
  if (aliases[a]) a = aliases[a]
  return a
}

var loaded = false
  , loading = false
  , loadErr = null
  , loadListeners = []

function loadCb (er) {
  loadListeners.forEach(function (cb) {
    process.nextTick(cb.bind(npm, er, npm))
  })
  loadListeners.length = 0
}


npm.load = function (conf, cb_) {
  if (!cb_ && typeof conf === "function") cb_ = conf , conf = {}
  if (!cb_) cb_ = function () {}
  if (!conf) conf = {}
  loadListeners.push(cb_)
  if (loaded || loadErr) return cb(loadErr)
  if (loading) return
  loading = true
  var onload = true

  function cb (er) {
    if (loadErr) return
    loaded = true
    loadCb(loadErr = er)
    if (onload = onload && npm.config.get("onload-script")) {
      require(onload)
      onload = false
    }
  }

  log.pause()

  load(npm, conf, cb)
}


function load (npm, conf, cb) {
  which(process.argv[0], function (er, node) {
    if (!er && node.toUpperCase() !== process.execPath.toUpperCase()) {
      log.verbose("node symlink", node)
      process.execPath = node
      process.installPrefix = path.resolve(node, "..", "..")
    }

    // look up configs
    //console.error("about to look up configs")

    ini.resolveConfigs(conf, function (er) {
      log.level = npm.config.get("loglevel")
      log.heading = "npm"
      log.stream = npm.config.get("logstream")
      switch (npm.config.get("color")) {
        case "always": log.enableColor(); break
        case false: log.disableColor(); break
      }
      log.resume()

      if (er) return cb(er)

      // at this point the configs are all set.
      // go ahead and spin up the registry client.
      npm.registry = new RegClient(
        { registry: npm.config.get("registry")
        , cache: npm.config.get("cache")
        , auth: npm.config.get("_auth")
        , alwaysAuth: npm.config.get("always-auth")
        , email: npm.config.get("email")
        , tag: npm.config.get("tag")
        , ca: npm.config.get("ca")
        , strictSSL: npm.config.get("strict-ssl")
        , userAgent: npm.config.get("user-agent")
        , E404: npm.E404
        , EPUBLISHCONFLICT: npm.EPUBLISHCONFLICT
        , log: log
        , retries: npm.config.get("fetch-retries")
        , retryFactor: npm.config.get("fetch-retry-factor")
        , retryMinTimeout: npm.config.get("fetch-retry-mintimeout")
        , retryMaxTimeout: npm.config.get("fetch-retry-maxtimeout")
        })

      var umask = parseInt(conf.umask, 8)
      npm.modes = { exec: 0777 & (~umask)
                  , file: 0666 & (~umask)
                  , umask: umask }

      chain([ [ loadPrefix, npm, conf ]
            , [ setUser, ini.configList, ini.defaultConfig ]
            , [ loadUid, npm, conf ]
            ], cb)
    })
  })
}

function loadPrefix (npm, conf, cb) {
  // try to guess at a good node_modules location.
  var p
    , gp
  if (!conf.hasOwnProperty("prefix")) {
    p = process.cwd()
  } else {
    p = npm.config.get("prefix")
  }
  gp = npm.config.get("prefix")

  findPrefix(p, function (er, p) {
    Object.defineProperty(npm, "localPrefix",
      { get : function () { return p }
      , set : function (r) { return p = r }
      , enumerable : true
      })
    // the prefix MUST exist, or else nothing works.
    if (!npm.config.get("global")) {
      mkdir(p, next)
    } else {
      next(er)
    }
  })

  findPrefix(gp, function (er, gp) {
    Object.defineProperty(npm, "globalPrefix",
      { get : function () { return gp }
      , set : function (r) { return gp = r }
      , enumerable : true
      })
    // the prefix MUST exist, or else nothing works.
    mkdir(gp, next)
  })

  var i = 2
    , errState = null
  function next (er) {
    if (errState) return
    if (er) return cb(errState = er)
    if (--i === 0) return cb()
  }
}


function loadUid (npm, conf, cb) {
  // if we're not in unsafe-perm mode, then figure out who
  // to run stuff as.  Do this first, to support `npm update npm -g`
  if (!npm.config.get("unsafe-perm")) {
    getUid(npm.config.get("user"), npm.config.get("group"), cb)
  } else {
    process.nextTick(cb)
  }
}

function setUser (cl, dc, cb) {
  // If global, leave it as-is.
  // If not global, then set the user to the owner of the prefix folder.
  // Just set the default, so it can be overridden.
  if (cl.get("global")) return cb()
  if (process.env.SUDO_UID) {
    dc.user = +(process.env.SUDO_UID)
    return cb()
  }

  var prefix = path.resolve(cl.get("prefix"))
  mkdir(prefix, function (er) {
    if (er) {
      log.error("could not create prefix dir", prefix)
      return cb(er)
    }
    fs.stat(prefix, function (er, st) {
      dc.user = st && st.uid
      return cb(er)
    })
  })
}


npm.config =
  { get : function (key) { return ini.get(key) }
  , set : function (key, val) { return ini.set(key, val, "cli") }
  , del : function (key, val) { return ini.del(key, val, "cli") }
  }

Object.defineProperty(npm, "prefix",
  { get : function () {
      return npm.config.get("global") ? npm.globalPrefix : npm.localPrefix
    }
  , set : function (r) {
      var k = npm.config.get("global") ? "globalPrefix" : "localPrefix"
      return npm[k] = r
    }
  , enumerable : true
  })

Object.defineProperty(npm, "bin",
  { get : function () {
      if (npm.config.get("global")) return npm.globalBin
      return path.resolve(npm.root, ".bin")
    }
  , enumerable : true
  })

Object.defineProperty(npm, "globalBin",
  { get : function () {
      var b = npm.globalPrefix
      if (process.platform !== "win32") b = path.resolve(b, "bin")
      return b
    }
  })

Object.defineProperty(npm, "dir",
  { get : function () {
      if (npm.config.get("global")) return npm.globalDir
      return path.resolve(npm.prefix, "node_modules")
    }
  , enumerable : true
  })

Object.defineProperty(npm, "globalDir",
  { get : function () {
      return (process.platform !== "win32")
           ? path.resolve(npm.globalPrefix, "lib", "node_modules")
           : path.resolve(npm.globalPrefix, "node_modules")
    }
  , enumerable : true
  })

Object.defineProperty(npm, "root",
  { get : function () { return npm.dir } })

Object.defineProperty(npm, "cache",
  { get : function () { return npm.config.get("cache") }
  , set : function (r) { return npm.config.set("cache", r) }
  , enumerable : true
  })

var tmpFolder
Object.defineProperty(npm, "tmp",
  { get : function () {
      if (!tmpFolder) tmpFolder = "npm-" + process.pid
      return path.resolve(npm.config.get("tmp"), tmpFolder)
    }
  , enumerable : true
  })

// the better to repl you with
Object.getOwnPropertyNames(npm.commands).forEach(function (n) {
  if (npm.hasOwnProperty(n)) return

  Object.defineProperty(npm, n, { get: function () {
    return function () {
      var args = Array.prototype.slice.call(arguments, 0)
        , cb = defaultCb

      if (args.length === 1 && Array.isArray(args[0])) {
        args = args[0]
      }

      if (typeof args[args.length - 1] === "function") {
        cb = args.pop()
      }

      npm.commands[n](args, cb)
    }
  }, enumerable: false, configurable: true })
})

if (require.main === module) {
  require("../bin/npm-cli.js")
}
})()
