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


// monkey-patch support for 0.6 child processes
require('child-process-close')

var EventEmitter = require("events").EventEmitter
  , npm = module.exports = new EventEmitter()
  , npmconf = require("npmconf")
  , log = require("npmlog")
  , fs = require("graceful-fs")
  , path = require("path")
  , abbrev = require("abbrev")
  , which = require("which")
  , semver = require("semver")
  , RegClient = require("npm-registry-client")
  , charSpin = require("char-spinner")

npm.config = {
  loaded: false,
  get: function() {
    throw new Error('npm.load() required')
  },
  set: function() {
    throw new Error('npm.load() required')
  }
}

npm.commands = {}

npm.rollbacks = []

try {
  var pv = process.version.replace(/^v/, '')
  // startup, ok to do this synchronously
  var j = JSON.parse(fs.readFileSync(
    path.join(__dirname, "../package.json"))+"")
  npm.version = j.version
  npm.nodeVersionRequired = j.engines.node
  if (!semver.satisfies(pv, j.engines.node)) {
    log.warn("unsupported version", [""
            ,"npm requires node version: "+j.engines.node
            ,"And you have: "+pv
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
              , "isntall" : "install"
              , "up" : "update"
              , "c" : "config"
              , "info" : "view"
              , "show" : "view"
              , "find" : "search"
              , "s" : "search"
              , "se" : "search"
              , "author" : "owner"
              , "home" : "docs"
              , "issues": "bugs"
              , "unstar": "star" // same function
              , "apihelp" : "help"
              , "login": "adduser"
              , "add-user": "adduser"
              , "tst": "test"
              , "t": "test"
              , "find-dupes": "dedupe"
              , "ddp": "dedupe"
              , "v": "view"
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
              , "dedupe"

              , "rebuild"
              , "link"

              , "publish"
              , "star"
              , "stars"
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
              , "repo"
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
               , "visnup"
               ]
  , fullList = npm.fullList = cmdList.concat(aliasNames).filter(function (c) {
      return plumbing.indexOf(c) === -1
    })
  , abbrevs = abbrev(fullList)

npm.spinner =
  { int: null
  , started: false
  , start: function () {
      if (npm.spinner.int) return
      var c = npm.config.get("spin")
      if (!c) return
      var stream = npm.config.get("logstream")
      var opt = { tty: c !== "always", stream: stream }
      opt.cleanup = !npm.spinner.started
      npm.spinner.int = charSpin(opt)
      npm.spinner.started = true
    }
  , stop: function () {
      clearInterval(npm.spinner.int)
      npm.spinner.int = null
    }
  }

Object.keys(abbrevs).concat(plumbing).forEach(function addCommand (c) {
  Object.defineProperty(npm.commands, c, { get : function () {
    if (!loaded) throw new Error(
      "Call npm.load(config, cb) before using this command.\n"+
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

      npm.registry.version = npm.version
      if (!npm.registry.refer) {
        npm.registry.refer = [a].concat(args[0]).map(function (arg) {
          // exclude anything that might be a URL, path, or private module
          // Those things will always have a slash in them somewhere
          if (arg && arg.match && arg.match(/\/|\\/)) {
            return "[REDACTED]"
          } else {
            return arg
          }
        }).filter(function (arg) {
          return arg && arg.match
        }).join(" ")
      }

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

npm.load = function (cli, cb_) {
  if (!cb_ && typeof cli === "function") cb_ = cli , cli = {}
  if (!cb_) cb_ = function () {}
  if (!cli) cli = {}
  loadListeners.push(cb_)
  if (loaded || loadErr) return cb(loadErr)
  if (loading) return
  loading = true
  var onload = true

  function cb (er) {
    if (loadErr) return
    loadErr = er
    if (er) return cb_(er)
    if (npm.config.get("force")) {
      log.warn("using --force", "I sure hope you know what you are doing.")
    }
    npm.config.loaded = true
    loaded = true
    loadCb(loadErr = er)
    if (onload = onload && npm.config.get("onload-script")) {
      require(onload)
      onload = false
    }
  }

  log.pause()

  load(npm, cli, cb)
}

function load (npm, cli, cb) {
  which(process.argv[0], function (er, node) {
    if (!er && node.toUpperCase() !== process.execPath.toUpperCase()) {
      log.verbose("node symlink", node)
      process.execPath = node
      process.installPrefix = path.resolve(node, "..", "..")
    }

    // look up configs
    //console.error("about to look up configs")

    var builtin = path.resolve(__dirname, "..", "npmrc")
    npmconf.load(cli, builtin, function (er, config) {
      if (er === config) er = null

      npm.config = config
      if (er) return cb(er)

      // if the "project" config is not a filename, and we're
      // not in global mode, then that means that it collided
      // with either the default or effective userland config
      if (!config.get("global")
          && config.sources.project
          && config.sources.project.type !== "ini") {
        log.verbose("config"
                   , "Skipping project config: %s. "
                   + "(matches userconfig)"
                   , config.localPrefix + "/.npmrc")
      }

      // Include npm-version and node-version in user-agent
      var ua = config.get("user-agent") || ""
      ua = ua.replace(/\{node-version\}/gi, process.version)
      ua = ua.replace(/\{npm-version\}/gi, npm.version)
      ua = ua.replace(/\{platform\}/gi, process.platform)
      ua = ua.replace(/\{arch\}/gi, process.arch)
      config.set("user-agent", ua)

      var color = config.get("color")

      log.level = config.get("loglevel")
      log.heading = config.get("heading") || "npm"
      log.stream = config.get("logstream")

      switch (color) {
        case "always":
          log.enableColor()
          npm.color = true
          break
        case false:
          log.disableColor()
          npm.color = false
          break
        default:
          var tty = require("tty")
          if (process.stdout.isTTY) npm.color = true
          else if (!tty.isatty) npm.color = true
          else if (tty.isatty(1)) npm.color = true
          else npm.color = false
          break
      }

      log.resume()

      // at this point the configs are all set.
      // go ahead and spin up the registry client.
      npm.registry = new RegClient(npm.config)

      var umask = npm.config.get("umask")
      npm.modes = { exec: 0777 & (~umask)
                  , file: 0666 & (~umask)
                  , umask: umask }

      var gp = Object.getOwnPropertyDescriptor(config, "globalPrefix")
      Object.defineProperty(npm, "globalPrefix", gp)

      var lp = Object.getOwnPropertyDescriptor(config, "localPrefix")
      Object.defineProperty(npm, "localPrefix", lp)

      return cb(null, npm)
    })
  })
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
var crypto = require("crypto")
var rand = crypto.randomBytes(6)
                 .toString("base64")
                 .replace(/\//g, '_')
                 .replace(/\+/, '-')
Object.defineProperty(npm, "tmp",
  { get : function () {
      if (!tmpFolder) tmpFolder = "npm-" + process.pid + "-" + rand
      return path.resolve(npm.config.get("tmp"), tmpFolder)
    }
  , enumerable : true
  })

// the better to repl you with
Object.getOwnPropertyNames(npm.commands).forEach(function (n) {
  if (npm.hasOwnProperty(n) || n === "config") return

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
