
exports = module.exports = lifecycle
exports.cmd = cmd

var log = require("npmlog")
  , exec = require("./exec.js")
  , npm = require("../npm.js")
  , path = require("path")
  , fs = require("graceful-fs")
  , chain = require("slide").chain
  , constants = require("constants")
  , Stream = require("stream").Stream
  , PATH = "PATH"

// windows calls it's path "Path" usually, but this is not guaranteed.
if (process.platform === "win32") {
  PATH = "Path"
  Object.keys(process.env).forEach(function (e) {
    if (e.match(/^PATH$/i)) {
      PATH = e
    }
  })
}

function lifecycle (pkg, stage, wd, unsafe, failOk, cb) {
  if (typeof cb !== "function") cb = failOk, failOk = false
  if (typeof cb !== "function") cb = unsafe, unsafe = false
  if (typeof cb !== "function") cb = wd, wd = null

  while (pkg && pkg._data) pkg = pkg._data
  if (!pkg) return cb(new Error("Invalid package data"))

  log.info(stage, pkg._id)
  if (!pkg.scripts) pkg.scripts = {}

  validWd(wd || path.resolve(npm.dir, pkg.name), function (er, wd) {
    if (er) return cb(er)

    unsafe = unsafe || npm.config.get("unsafe-perm")

    if ((wd.indexOf(npm.dir) !== 0 || path.basename(wd) !== pkg.name)
        && !unsafe && pkg.scripts[stage]) {
      log.warn( "cannot run in wd", "%s %s (wd=%s)"
              , pkg._id, pkg.scripts[stage], wd)
      return cb()
    }

    // set the env variables, then run scripts as a child process.
    var env = makeEnv(pkg)
    env.npm_lifecycle_event = stage
    env.npm_node_execpath = env.NODE = env.NODE || process.execPath

    // "nobody" typically doesn't have permission to write to /tmp
    // even if it's never used, sh freaks out.
    if (!npm.config.get("unsafe-perm")) env.TMPDIR = wd

    lifecycle_(pkg, stage, wd, env, unsafe, failOk, cb)
  })
}

function checkForLink (pkg, cb) {
  var f = path.join(npm.dir, pkg.name)
  fs.lstat(f, function (er, s) {
    cb(null, !(er || !s.isSymbolicLink()))
  })
}

function lifecycle_ (pkg, stage, wd, env, unsafe, failOk, cb) {
  var pathArr = []
    , p = wd.split("node_modules")
    , acc = path.resolve(p.shift())
  p.forEach(function (pp) {
    pathArr.unshift(path.join(acc, "node_modules", ".bin"))
    acc = path.join(acc, "node_modules", pp)
  })
  pathArr.unshift(path.join(acc, "node_modules", ".bin"))

  // we also unshift the bundled node-gyp-bin folder so that
  // the bundled one will be used for installing things.
  pathArr.unshift(path.join(__dirname, "..", "..", "bin", "node-gyp-bin"))

  // add the directory containing the `node` executable currently running, so
  // that any lifecycle script that invoke "node" will execute this same one.
  pathArr.unshift(path.dirname(process.execPath))

  if (env[PATH]) pathArr.push(env[PATH])
  env[PATH] = pathArr.join(process.platform === "win32" ? ";" : ":")

  var packageLifecycle = pkg.scripts && pkg.scripts.hasOwnProperty(stage)

  if (packageLifecycle) {
    // define this here so it's available to all scripts.
    env.npm_lifecycle_script = pkg.scripts[stage]
  }

  if (failOk) {
    cb = (function (cb_) { return function (er) {
      if (er) log.warn("continuing anyway", er.message)
      cb_()
    }})(cb)
  }

  if (npm.config.get("force")) {
    cb = (function (cb_) { return function (er) {
      if (er) log.info("forced, continuing", er)
      cb_()
    }})(cb)
  }

  chain
    ( [ packageLifecycle && [runPackageLifecycle, pkg, env, wd, unsafe]
      , [runHookLifecycle, pkg, env, wd, unsafe] ]
    , cb )
}

function validWd (d, cb) {
  fs.stat(d, function (er, st) {
    if (er || !st.isDirectory()) {
      var p = path.dirname(d)
      if (p === d) {
        return cb(new Error("Could not find suitable wd"))
      }
      return validWd(p, cb)
    }
    return cb(null, d)
  })
}

function runPackageLifecycle (pkg, env, wd, unsafe, cb) {
  // run package lifecycle scripts in the package root, or the nearest parent.
  var stage = env.npm_lifecycle_event
    , user = unsafe ? null : npm.config.get("user")
    , group = unsafe ? null : npm.config.get("group")
    , cmd = env.npm_lifecycle_script
    , sh = "sh"
    , shFlag = "-c"

  if (process.platform === "win32") {
    sh = "cmd"
    shFlag = "/c"
  }

  log.verbose("unsafe-perm in lifecycle", unsafe)

  var note = "\n> " + pkg._id + " " + stage + " " + wd
           + "\n> " + cmd + "\n"

  console.log(note)
  exec( sh, [shFlag, cmd], env, true, wd
      , user, group
      , function (er, code, stdout, stderr) {
    if (er && !npm.ROLLBACK) {
      log.info(pkg._id, "Failed to exec "+stage+" script")
      er.message = pkg._id + " "
                 + stage + ": `" + env.npm_lifecycle_script+"`\n"
                 + er.message
      if (er.code !== "EPERM") {
        er.code = "ELIFECYCLE"
      }
      er.pkgid = pkg._id
      er.stage = stage
      er.script = env.npm_lifecycle_script
      er.pkgname = pkg.name
      return cb(er)
    } else if (er) {
      log.error(pkg._id+"."+stage, er)
      log.error(pkg._id+"."+stage, "continuing anyway")
      return cb()
    }
    cb(er)
  })
}

function runHookLifecycle (pkg, env, wd, unsafe, cb) {
  // check for a hook script, run if present.
  var stage = env.npm_lifecycle_event
    , hook = path.join(npm.dir, ".hooks", stage)
    , user = unsafe ? null : npm.config.get("user")
    , group = unsafe ? null : npm.config.get("group")
    , cmd = hook

  fs.stat(hook, function (er) {
    if (er) return cb()

    exec( "sh", ["-c", cmd], env, true, wd
        , user, group
        , function (er) {
      if (er) {
        er.message += "\nFailed to exec "+stage+" hook script"
        log.info(pkg._id, er)
      }
      if (npm.ROLLBACK) return cb()
      cb(er)
    })
  })
}

function makeEnv (data, prefix, env) {
  prefix = prefix || "npm_package_"
  if (!env) {
    env = {}
    for (var i in process.env) if (!i.match(/^npm_/)) {
      env[i] = process.env[i]
    }

    // npat asks for tap output
    if (npm.config.get("npat")) env.TAP = 1

    // express and others respect the NODE_ENV value.
    if (npm.config.get("production")) env.NODE_ENV = "production"

  } else if (!data.hasOwnProperty("_lifecycleEnv")) {
    Object.defineProperty(data, "_lifecycleEnv",
      { value : env
      , enumerable : false
      })
  }

  for (var i in data) if (i.charAt(0) !== "_") {
    var envKey = (prefix+i).replace(/[^a-zA-Z0-9_]/g, '_')
    if (data[i] && typeof(data[i]) === "object") {
      try {
        // quick and dirty detection for cyclical structures
        JSON.stringify(data[i])
        makeEnv(data[i], envKey+"_", env)
      } catch (ex) {
        // usually these are package objects.
        // just get the path and basic details.
        var d = data[i]
        makeEnv( { name: d.name, version: d.version, path:d.path }
               , envKey+"_", env)
      }
    } else {
      env[envKey] = String(data[i])
      env[envKey] = -1 !== env[envKey].indexOf("\n")
                  ? JSON.stringify(env[envKey])
                  : env[envKey]
    }

  }

  if (prefix !== "npm_package_") return env

  prefix = "npm_config_"
  var pkgConfig = {}
    , keys = npm.config.keys
    , pkgVerConfig = {}
    , namePref = data.name + ":"
    , verPref = data.name + "@" + data.version + ":"

  keys.forEach(function (i) {
    if (i.charAt(0) === "_" && i.indexOf("_"+namePref) !== 0) {
      return
    }
    var value = npm.config.get(i)
    if (value instanceof Stream || Array.isArray(value)) return
    if (!value) value = ""
    else if (typeof value !== "string") value = JSON.stringify(value)

    value = -1 !== value.indexOf("\n")
          ? JSON.stringify(value)
          : value
    i = i.replace(/^_+/, "")
    if (i.indexOf(namePref) === 0) {
      var k = i.substr(namePref.length).replace(/[^a-zA-Z0-9_]/g, "_")
      pkgConfig[ k ] = value
    } else if (i.indexOf(verPref) === 0) {
      var k = i.substr(verPref.length).replace(/[^a-zA-Z0-9_]/g, "_")
      pkgVerConfig[ k ] = value
    }
    var envKey = (prefix+i).replace(/[^a-zA-Z0-9_]/g, "_")
    env[envKey] = value
  })

  prefix = "npm_package_config_"
  ;[pkgConfig, pkgVerConfig].forEach(function (conf) {
    for (var i in conf) {
      var envKey = (prefix+i)
      env[envKey] = conf[i]
    }
  })

  return env
}

function cmd (stage) {
  function CMD (args, cb) {
    if (args.length) {
      chain(args.map(function (p) {
        return [npm.commands, "run-script", [p, stage]]
      }), cb)
    } else npm.commands["run-script"]([stage], cb)
  }
  CMD.usage = "npm "+stage+" <name>"
  var installedShallow = require("./completion/installed-shallow.js")
  CMD.completion = function (opts, cb) {
    installedShallow(opts, function (d) {
      return d.scripts && d.scripts[stage]
    }, cb)
  }
  return CMD
}
