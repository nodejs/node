
module.exports = errorHandler

var cbCalled = false
  , log = require("npmlog")
  , npm = require("../npm.js")
  , rm = require("rimraf")
  , itWorked = false
  , path = require("path")
  , wroteLogFile = false
  , exitCode = 0
  , rollbacks = npm.rollbacks
  , chain = require("slide").chain
  , writeStream = require("fs-write-stream-atomic")
  , nameValidator = require("validate-npm-package-name")


process.on("exit", function (code) {
  // console.error("exit", code)
  if (!npm.config || !npm.config.loaded) return
  if (code) itWorked = false
  if (itWorked) log.info("ok")
  else {
    if (!cbCalled) {
      log.error("", "cb() never called!")
    }

    if (wroteLogFile) {
      // just a line break
      if (log.levels[log.level] <= log.levels.error) console.error("")

      log.error("",
                ["Please include the following file with any support request:"
                ,"    " + path.resolve("npm-debug.log")
                ].join("\n"))
      wroteLogFile = false
    }
    if (code) {
      log.error("code", code)
    }
  }

  var doExit = npm.config.get("_exit")
  if (doExit) {
    // actually exit.
    if (exitCode === 0 && !itWorked) {
      exitCode = 1
    }
    if (exitCode !== 0) process.exit(exitCode)
  } else {
    itWorked = false // ready for next exit
  }
})

function exit (code, noLog) {
  exitCode = exitCode || process.exitCode || code

  var doExit = npm.config ? npm.config.get("_exit") : true
  log.verbose("exit", [code, doExit])
  if (log.level === "silent") noLog = true

  if (rollbacks.length) {
    chain(rollbacks.map(function (f) {
      return function (cb) {
        npm.commands.unbuild([f], true, cb)
      }
    }), function (er) {
      if (er) {
        log.error("error rolling back", er)
        if (!code) errorHandler(er)
        else if (noLog) rm("npm-debug.log", reallyExit.bind(null, er))
        else writeLogFile(reallyExit.bind(this, er))
      } else {
        if (!noLog && code) writeLogFile(reallyExit)
        else rm("npm-debug.log", reallyExit)
      }
    })
    rollbacks.length = 0
  }
  else if (code && !noLog) writeLogFile(reallyExit)
  else rm("npm-debug.log", reallyExit)

  function reallyExit (er) {
    if (er && !code) code = typeof er.errno === "number" ? er.errno : 1

    // truncate once it's been written.
    log.record.length = 0

    itWorked = !code

    // just emit a fake exit event.
    // if we're really exiting, then let it exit on its own, so that
    // in-process stuff can finish or clean up first.
    if (!doExit) process.emit("exit", code)
    npm.spinner.stop()
  }
}


function errorHandler (er) {
  // console.error("errorHandler", er)
  if (!npm.config || !npm.config.loaded) {
    // logging won't work unless we pretend that it's ready
    er = er || new Error("Exit prior to config file resolving.")
    console.error(er.stack || er.message)
  }

  if (cbCalled) {
    er = er || new Error("Callback called more than once.")
  }

  cbCalled = true
  if (!er) return exit(0)
  if (typeof er === "string") {
    log.error("", er)
    return exit(1, true)
  } else if (!(er instanceof Error)) {
    log.error("weird error", er)
    return exit(1, true)
  }

  var m = er.code || er.message.match(/^(?:Error: )?(E[A-Z]+)/)
  if (m && !er.code) er.code = m

  ; [ "type"
    , "fstream_path"
    , "fstream_unc_path"
    , "fstream_type"
    , "fstream_class"
    , "fstream_finish_call"
    , "fstream_linkpath"
    , "stack"
    , "fstream_stack"
    , "statusCode"
    , "pkgid"
    ].forEach(function (k) {
      var v = er[k]
      if (!v) return
      if (k === "fstream_stack") v = v.join("\n")
      log.verbose(k, v)
    })

  log.verbose("cwd", process.cwd())

  var os = require("os")
  // log.error("System", os.type() + " " + os.release())
  // log.error("command", process.argv.map(JSON.stringify).join(" "))
  // log.error("node -v", process.version)
  // log.error("npm -v", npm.version)
  log.error("", os.type() + " " + os.release())
  log.error("argv", process.argv.map(JSON.stringify).join(" "))
  log.error("node", process.version)
  log.error("npm ", "v" + npm.version)

  ; [ "file"
    , "path"
    , "code"
    , "errno"
    , "syscall"
    ].forEach(function (k) {
      var v = er[k]
      if (v) log.error(k, v)
    })

  // just a line break
  if (log.levels[log.level] <= log.levels.error) console.error("")

  switch (er.code) {
  case "ECONNREFUSED":
    log.error("", er)
    log.error("", ["\nIf you are behind a proxy, please make sure that the"
              ,"'proxy' config is set properly.  See: 'npm help config'"
              ].join("\n"))
    break

  case "EACCES":
  case "EPERM":
    log.error("", er)
    log.error("", ["\nPlease try running this command again as root/Administrator."
              ].join("\n"))
    break

  case "ELIFECYCLE":
    log.error("", er.message)
    log.error("", ["","Failed at the "+er.pkgid+" "+er.stage+" script '"+er.script+"'."
              ,"This is most likely a problem with the "+er.pkgname+" package,"
              ,"not with npm itself."
              ,"Tell the author that this fails on your system:"
              ,"    "+er.script
              ,"You can get their info via:"
              ,"    npm owner ls "+er.pkgname
              ,"There is likely additional logging output above."
              ].join("\n"))
    break

  case "ENOGIT":
    log.error("", er.message)
    log.error("", ["","Failed using git."
              ,"This is most likely not a problem with npm itself."
              ,"Please check if you have git installed and in your PATH."
              ].join("\n"))
    break

  case "EJSONPARSE":
    log.error("", er.message)
    log.error("", "File: "+er.file)
    log.error("", ["Failed to parse package.json data."
              ,"package.json must be actual JSON, not just JavaScript."
              ,"","This is not a bug in npm."
              ,"Tell the package author to fix their package.json file."
              ].join("\n"), "JSON.parse")
    break

  // TODO(isaacs)
  // Add a special case here for E401 and E403 explaining auth issues?

  case "E404":
    var msg = [er.message]
    if (er.pkgid && er.pkgid !== "-") {
      msg.push("", "'" + er.pkgid + "' is not in the npm registry.")

      var valResult = nameValidator(er.pkgid)

      if (valResult.validForNewPackages) {
        msg.push("You should bug the author to publish it (or use the name yourself!)")
      } else {
        msg.push("Your package name is not valid, because", "")

        var errorsArray = (valResult.errors || []).concat(valResult.warnings || [])
        errorsArray.forEach(function(item, idx) {
          msg.push(" " + (idx + 1) + ". " + item)
        })
      }

      if (er.parent) {
        msg.push("It was specified as a dependency of '"+er.parent+"'")
      }
      msg.push("\nNote that you can also install from a"
              ,"tarball, folder, http url, or git url.")
    }
    // There's no need to have 404 in the message as well.
    msg[0] = msg[0].replace(/^404\s+/, "")
    log.error("404", msg.join("\n"))
    break

  case "EPUBLISHCONFLICT":
    log.error("publish fail", ["Cannot publish over existing version."
              ,"Update the 'version' field in package.json and try again."
              ,""
              ,"To automatically increment version numbers, see:"
              ,"    npm help version"
              ].join("\n"))
    break

  case "EISGIT":
    log.error("git", [er.message
              ,"    "+er.path
              ,"Refusing to remove it. Update manually,"
              ,"or move it out of the way first."
              ].join("\n"))
    break

  case "ECYCLE":
    log.error("cycle", [er.message
              ,"While installing: "+er.pkgid
              ,"Found a pathological dependency case that npm cannot solve."
              ,"Please report this to the package author."
              ].join("\n"))
    break

  case "EBADPLATFORM":
    log.error("notsup", [er.message
              ,"Not compatible with your operating system or architecture: "+er.pkgid
              ,"Valid OS:    "+er.os.join(",")
              ,"Valid Arch:  "+er.cpu.join(",")
              ,"Actual OS:   "+process.platform
              ,"Actual Arch: "+process.arch
              ].join("\n"))
    break

  case "EEXIST":
    log.error([er.message
              ,"File exists: "+er.path
              ,"Move it away, and try again."].join("\n"))
    break

  case "ENEEDAUTH":
    log.error("need auth", [er.message
              ,"You need to authorize this machine using `npm adduser`"
              ].join("\n"))
    break

  case "EPEERINVALID":
    var peerErrors = Object.keys(er.peersDepending).map(function (peer) {
      return "Peer " + peer + " wants " + er.packageName + "@"
        + er.peersDepending[peer]
    })
    log.error("peerinvalid", [er.message].concat(peerErrors).join("\n"))
    break

  case "ECONNRESET":
  case "ENOTFOUND":
  case "ETIMEDOUT":
  case "EAI_FAIL":
    log.error("network", [er.message
              ,"This is most likely not a problem with npm itself"
              ,"and is related to network connectivity."
              ,"In most cases you are behind a proxy or have bad network settings."
              ,"\nIf you are behind a proxy, please make sure that the"
              ,"'proxy' config is set properly.  See: 'npm help config'"
              ].join("\n"))
    break

  case "ENOPACKAGEJSON":
    log.error("package.json", [er.message
              ,"This is most likely not a problem with npm itself."
              ,"npm can't find a package.json file in your current directory."
              ].join("\n"))
    break

  case "ETARGET":
    var msg = [er.message
              ,"This is most likely not a problem with npm itself."
              ,"In most cases you or one of your dependencies are requesting"
              ,"a package version that doesn't exist."
              ]
      if (er.parent) {
        msg.push("\nIt was specified as a dependency of '"+er.parent+"'\n")
      }
      log.error("notarget", msg.join("\n"))
    break

  case "ENOTSUP":
    if (er.required) {
      log.error("notsup", [er.message
                ,"Not compatible with your version of node/npm: "+er.pkgid
                ,"Required: "+JSON.stringify(er.required)
                ,"Actual:   "
                +JSON.stringify({npm:npm.version
                                ,node:npm.config.get("node-version")})
                ].join("\n"))
      break
    } // else passthrough

  case "ENOSPC":
    log.error("nospc", [er.message
              ,"This is most likely not a problem with npm itself"
              ,"and is related to insufficient space on your system."
              ].join("\n"))
    break

  case "EROFS":
    log.error("rofs", [er.message
              ,"This is most likely not a problem with npm itself"
              ,"and is related to the file system being read-only."
              ,"\nOften virtualized file systems, or other file systems"
              ,"that don't support symlinks, give this error."
              ].join("\n"))
    break

  case "ENOENT":
    log.error("enoent", [er.message
              ,"This is most likely not a problem with npm itself"
              ,"and is related to npm not being able to find a file."
              ,er.file?"\nCheck if the file '"+er.file+"' is present.":""
              ].join("\n"))
    break

  default:
    log.error("", er.message || er)
    log.error("", ["", "If you need help, you may report this error at:"
                  ,"    <https://github.com/npm/npm/issues>"
                  ].join("\n"))
    break
  }

  exit(typeof er.errno === "number" ? er.errno : 1)
}

var writingLogFile = false
function writeLogFile (cb) {
  if (writingLogFile) return cb()
  writingLogFile = true
  wroteLogFile = true

  var fstr = writeStream("npm-debug.log")
    , os = require("os")
    , out = ""

  log.record.forEach(function (m) {
    var pref = [m.id, m.level]
    if (m.prefix) pref.push(m.prefix)
    pref = pref.join(" ")

    m.message.trim().split(/\r?\n/).map(function (line) {
      return (pref + " " + line).trim()
    }).forEach(function (line) {
      out += line + os.EOL
    })
  })

  fstr.end(out)
  fstr.on("close", cb)
}
