
module.exports = config

config.usage = "npm config set <key> <value>"
             + "\nnpm config get [<key>]"
             + "\nnpm config delete <key>"
             + "\nnpm config list"
             + "\nnpm config edit"
             + "\nnpm set <key> <value>"
             + "\nnpm get [<key>]"

var log = require("npmlog")
  , npm = require("./npm.js")
  , spawn = require("child_process").spawn
  , fs = require("graceful-fs")
  , npmconf = require("npmconf")
  , types = npmconf.defs.types
  , ini = require("ini")
  , editor = require("editor")
  , os = require("os")

config.completion = function (opts, cb) {
  var argv = opts.conf.argv.remain
  if (argv[1] !== "config") argv.unshift("config")
  if (argv.length === 2) {
    var cmds = ["get", "set", "delete", "ls", "rm", "edit"]
    if (opts.partialWord !== "l") cmds.push("list")
    return cb(null, cmds)
  }

  var action = argv[2]
  switch (action) {
    case "set":
      // todo: complete with valid values, if possible.
      if (argv.length > 3) return cb(null, [])
      // fallthrough
    case "get":
    case "delete":
    case "rm":
      return cb(null, Object.keys(types))
    case "edit":
    case "list": case "ls":
      return cb(null, [])
    default: return cb(null, [])
  }
}

// npm config set key value
// npm config get key
// npm config list
function config (args, cb) {
  var action = args.shift()
  switch (action) {
    case "set": return set(args[0], args[1], cb)
    case "get": return get(args[0], cb)
    case "delete": case "rm": case "del": return del(args[0], cb)
    case "list": case "ls": return list(cb)
    case "edit": return edit(cb)
    default: return unknown(action, cb)
  }
}

function edit (cb) {
  var e = npm.config.get("editor")
    , which = npm.config.get("global") ? "global" : "user"
    , f = npm.config.get(which + "config")
  if (!e) return cb(new Error("No EDITOR config or environ set."))
  npm.config.save(which, function (er) {
    if (er) return cb(er)
    fs.readFile(f, "utf8", function (er, data) {
      if (er) data = ""
      data = [ ";;;;"
             , "; npm "+(npm.config.get("global") ?
                         "globalconfig" : "userconfig")+" file"
             , "; this is a simple ini-formatted file"
             , "; lines that start with semi-colons are comments."
             , "; read `npm help config` for help on the various options"
             , ";;;;"
             , ""
             , data
             ].concat( [ ";;;;"
                       , "; all options with default values"
                       , ";;;;"
                       ]
                     )
              .concat(Object.keys(npmconf.defaults).reduce(function (arr, key) {
                var obj = {};
                obj[key] = npmconf.defaults[key]
                if (key === "logstream") return arr
                return arr.concat(
                  ini.stringify(obj)
                    .replace(/\n$/m, '')
                    .replace(/^/g, '; ')
                    .replace(/\n/g, '\n; ')
                    .split('\n'))
              }, []))
              .concat([""])
              .join(os.EOL)
      fs.writeFile
        ( f
        , data
        , "utf8"
        , function (er) {
            if (er) return cb(er)
            editor(f, { editor: e }, cb)
          }
        )
    })
  })
}

function del (key, cb) {
  if (!key) return cb(new Error("no key provided"))
  var where = npm.config.get("global") ? "global" : "user"
  npm.config.del(key, where)
  npm.config.save(where, cb)
}

function set (key, val, cb) {
  if (key === undefined) {
    return unknown("", cb)
  }
  if (val === undefined) {
    if (key.indexOf("=") !== -1) {
      var k = key.split("=")
      key = k.shift()
      val = k.join("=")
    } else {
      val = ""
    }
  }
  key = key.trim()
  val = val.trim()
  log.info("config", "set %j %j", key, val)
  var where = npm.config.get("global") ? "global" : "user"
  npm.config.set(key, val, where)
  npm.config.save(where, cb)
}

function get (key, cb) {
  if (!key) return list(cb)
  if (key.charAt(0) === "_") {
    return cb(new Error("---sekretz---"))
  }
  console.log(npm.config.get(key))
  cb()
}

function sort (a, b) {
  return a > b ? 1 : -1
}

function reverse (a, b) {
  return a > b ? -1 : 1
}

function public (k) {
  return !(k.charAt(0) === "_" || types[k] !== types[k])
}

function getKeys (data) {
  return Object.keys(data).filter(public).sort(sort)
}

function list (cb) {
  var msg = ""
    , long = npm.config.get("long")

  var cli = npm.config.sources.cli.data
    , cliKeys = getKeys(cli)
  if (cliKeys.length) {
    msg += "; cli configs\n"
    cliKeys.forEach(function (k) {
      if (cli[k] && typeof cli[k] === "object") return
      if (k === "argv") return
      msg += k + " = " + JSON.stringify(cli[k]) + "\n"
    })
    msg += "\n"
  }

  // env configs
  var env = npm.config.sources.env.data
    , envKeys = getKeys(env)
  if (envKeys.length) {
    msg += "; environment configs\n"
    envKeys.forEach(function (k) {
      if (env[k] !== npm.config.get(k)) {
        if (!long) return
        msg += "; " + k + " = " + JSON.stringify(env[k])
            + " (overridden)\n"
      } else msg += k + " = " + JSON.stringify(env[k]) + "\n"
    })
    msg += "\n"
  }

  // user config file
  var uconf = npm.config.sources.user.data
    , uconfKeys = getKeys(uconf)
  if (uconfKeys.length) {
    msg += "; userconfig " + npm.config.get("userconfig") + "\n"
    uconfKeys.forEach(function (k) {
      var val = (k.charAt(0) === "_")
              ? "---sekretz---"
              : JSON.stringify(uconf[k])
      if (uconf[k] !== npm.config.get(k)) {
        if (!long) return
        msg += "; " + k + " = " + val
            + " (overridden)\n"
      } else msg += k + " = " + val + "\n"
    })
    msg += "\n"
  }

  // global config file
  var gconf = npm.config.sources.global.data
    , gconfKeys = getKeys(gconf)
  if (gconfKeys.length) {
    msg += "; globalconfig " + npm.config.get("globalconfig") + "\n"
    gconfKeys.forEach(function (k) {
      var val = (k.charAt(0) === "_")
              ? "---sekretz---"
              : JSON.stringify(gconf[k])
      if (gconf[k] !== npm.config.get(k)) {
        if (!long) return
        msg += "; " + k + " = " + val
            + " (overridden)\n"
      } else msg += k + " = " + val + "\n"
    })
    msg += "\n"
  }

  // builtin config file
  var builtin = npm.config.sources.builtin || {}
  if (builtin && builtin.data) {
    var bconf = builtin.data
      , bpath = builtin.path
      , bconfKeys = getKeys(bconf)
    if (bconfKeys.length) {
      var path = require("path")
      msg += "; builtin config " + bpath + "\n"
      bconfKeys.forEach(function (k) {
        var val = (k.charAt(0) === "_")
                ? "---sekretz---"
                : JSON.stringify(bconf[k])
        if (bconf[k] !== npm.config.get(k)) {
          if (!long) return
          msg += "; " + k + " = " + val
              + " (overridden)\n"
        } else msg += k + " = " + val + "\n"
      })
      msg += "\n"
    }
  }

  // only show defaults if --long
  if (!long) {
    msg += "; node bin location = " + process.execPath + "\n"
         + "; cwd = " + process.cwd() + "\n"
         + "; HOME = " + process.env.HOME + "\n"
         + "; 'npm config ls -l' to show all defaults.\n"

    console.log(msg)
    return cb()
  }

  var defaults = npmconf.defaults
    , defKeys = getKeys(defaults)
  msg += "; default values\n"
  defKeys.forEach(function (k) {
    if (defaults[k] && typeof defaults[k] === "object") return
    var val = JSON.stringify(defaults[k])
    if (defaults[k] !== npm.config.get(k)) {
      msg += "; " + k + " = " + val
          + " (overridden)\n"
    } else msg += k + " = " + val + "\n"
  })
  msg += "\n"

  console.log(msg)
  return cb()
}

function unknown (action, cb) {
  cb("Usage:\n" + config.usage)
}
