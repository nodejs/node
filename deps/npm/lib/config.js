
module.exports = config

config.usage = "npm config set <key> <value>"
             + "\nnpm config get <key>"
             + "\nnpm config delete <key>"
             + "\nnpm config list"
             + "\nnpm config edit"

var ini = require("./utils/ini.js")
  , log = require("./utils/log.js")
  , npm = require("./npm.js")
  , exec = require("./utils/exec.js")
  , fs = require("graceful-fs")
  , dc
  , output = require("./utils/output.js")
  , types = require("./utils/config-defs.js").types

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
  var e = ini.get("editor")
    , which = ini.get("global") ? "global" : "user"
    , f = ini.get(which + "config")
    , eol = process.platform === "win32" ? "\r\n" : "\n"
  if (!e) return cb(new Error("No EDITOR config or environ set."))
  ini.save(which, function (er) {
    if (er) return cb(er)
    fs.readFile(f, "utf8", function (er, data) {
      if (er) data = ""
      dc = dc || require("./utils/config-defs.js").defaults
      data = [ ";;;;"
             , "; npm "+(ini.get("global") ?
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
              .concat(Object.keys(dc).map(function (k) {
                return "; " + k + " = " + ini.unParseField(dc[k],k)
              }))
              .concat([""])
              .join(eol)
      fs.writeFile
        ( f
        , data
        , "utf8"
        , function (er) {
            if (er) return cb(er)
            exec(e, [f], function (er) {
              if (er) return cb(er)
              ini.resolveConfigs(function (er) {
                if (er) return cb(er)
                ini.save(which, cb)
              })
            })
          }
        )
    })
  })
}

function del (key, cb) {
  if (!key) return cb(new Error("no key provided"))
  ini.del(key)
  ini.save(ini.get("global") ? "global" : "user", cb)
}

function set (key, val, cb) {
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
  log("set "+key+" "+val, "config")
  var where = ini.get("global") ? "global" : "user"
  ini.set(key, val, where)
  ini.save(where, cb)
}

function get (key, cb) {
  if (!key) return list(cb)
  if (key.charAt(0) === "_") {
    return cb(new Error("---sekretz---"))
  }
  output.write(npm.config.get(key), cb)
}

function sort (a, b) {
  return a > b ? 1 : -1
}

function reverse (a, b) {
  return a > b ? -1 : 1
}

function list (cb) {
  var msg = ""
    , long = npm.config.get("long")

  // cli configs.
  // show any that aren't secret
  var cli = ini.configList.list[ini.TRANS.cli]
    , eol = process.platform === "win32" ? "\r\n" : "\n"
    , cliKeys = Object.keys(cli).filter(function (k) {
        return !(k.charAt(0) === "_" || types[k] !== types[k])
      }).sort(function (a, b) {
        return a > b ? 1 : -1
      })
  if (cliKeys.length) {
    msg += "; cli configs" + eol
    cliKeys.forEach(function (k) {
      if (k === "argv") return
      msg += k + " = " + JSON.stringify(cli[k]) + eol
    })
    msg += eol
  }

  // env configs
  var env = ini.configList.list[ini.TRANS.env]
    , envKeys = Object.keys(env).filter(function (k) {
        return !(k.charAt(0) === "_" || types[k] !== types[k])
      }).sort(function (a, b) {
        return a > b ? 1 : -1
      })
  if (envKeys.length) {
    msg += "; environment configs" + eol
    envKeys.forEach(function (k) {
      if (env[k] !== ini.get(k)) {
        if (!long) return
        msg += "; " + k + " = " + JSON.stringify(env[k])
            + " (overridden)" + eol
      } else msg += k + " = " + JSON.stringify(env[k]) + eol
    })
    msg += eol
  }

  // user config file
  var uconf = ini.configList.list[ini.TRANS.user]
    , uconfKeys = Object.keys(uconf).filter(function (k) {
        return types[k] === types[k]
      }).sort(function (a, b) {
        return a > b ? 1 : -1
      })
  if (uconfKeys.length) {
    msg += "; userconfig " + ini.get("userconfig") + eol
    uconfKeys.forEach(function (k) {
      var val = (k.charAt(0) === "_")
              ? "---sekretz---"
              : JSON.stringify(uconf[k])
      if (uconf[k] !== ini.get(k)) {
        if (!long) return
        msg += "; " + k + " = " + val
            + " (overridden)" + eol
      } else msg += k + " = " + val + eol
    })
    msg += eol
  }

  // global config file
  var gconf = ini.configList.list[ini.TRANS.global]
    , gconfKeys = Object.keys(gconf).filter(function (k) {
        return types[k] === types[k]
      }).sort(function (a, b) {
        return a > b ? 1 : -1
      })
  if (gconfKeys.length) {
    msg += "; globalconfig " + ini.get("globalconfig") + eol
    gconfKeys.forEach(function (k) {
      var val = (k.charAt(0) === "_")
              ? "---sekretz---"
              : JSON.stringify(gconf[k])
      if (gconf[k] !== ini.get(k)) {
        if (!long) return
        msg += "; " + k + " = " + val
            + " (overridden)" + eol
      } else msg += k + " = " + val + eol
    })
    msg += eol
  }

  // builtin config file
  var bconf = ini.configList.list[ini.TRANS.builtin]
    , bconfKeys = Object.keys(bconf).filter(function (k) {
        return types[k] === types[k]
      }).sort(function (a, b) {
        return a > b ? 1 : -1
      })
  if (bconfKeys.length) {
    var path = require("path")
    msg += "; builtin config " + path.resolve(__dirname, "../npmrc") + eol
    bconfKeys.forEach(function (k) {
      var val = (k.charAt(0) === "_")
              ? "---sekretz---"
              : JSON.stringify(bconf[k])
      if (bconf[k] !== ini.get(k)) {
        if (!long) return
        msg += "; " + k + " = " + val
            + " (overridden)" + eol
      } else msg += k + " = " + val + eol
    })
    msg += eol
  }

  // only show defaults if --long
  if (!long) {
    msg += "; node install prefix = " + process.installPrefix + eol
         + "; node bin location = " + process.execPath + eol
         + "; cwd = " + process.cwd() + eol
         + "; HOME = " + process.env.HOME + eol
         + "; 'npm config ls -l' to show all defaults." + eol

    return output.write(msg, cb)
  }

  var defaults = ini.defaultConfig
    , defKeys = Object.keys(defaults)
  msg += "; default values" + eol
  defKeys.forEach(function (k) {
    var val = JSON.stringify(defaults[k])
    if (defaults[k] !== ini.get(k)) {
      if (!long) return
      msg += "; " + k + " = " + val
          + " (overridden)" + eol
    } else msg += k + " = " + val + eol
  })
  msg += eol

  return output.write(msg, cb)
}

function unknown (action, cb) {
  cb("Usage:\n" + config.usage)
}
