
module.exports = errorHandler

var cbCalled = false
  , log = require("./log.js")
  , npm = require("../npm.js")
  , rm = require("rimraf")
  , constants = require("constants")
  , itWorked = false
  , path = require("path")
  , ini = require("./ini.js")
  , wroteLogFile = false


process.on("exit", function (code) {
  // console.error("exit", code)
  if (!ini.resolved) return
  if (code) itWorked = false
  if (itWorked) log("ok")
  else {
    if (!cbCalled) {
      log.error("cb() never called!\n ")
    }
    if (wroteLogFile) {
      log.error([""
                ,"Additional logging details can be found in:"
                ,"    " + path.resolve("npm-debug.log")
                ].join("\n"))
      wroteLogFile = false
    }
    log.win("not ok")
  }
  itWorked = false // ready for next exit
})

function errorHandler (er) {
  // console.error("errorHandler", er)
  if (!ini.resolved) {
    // logging won't work unless we pretend that it's ready
    er = er || new Error("Exit prior to config file resolving.")
    console.error(er.stack || er.message)
  }

  if (cbCalled) {
    er = er || new Error("Callback called more than once.")
  }

  cbCalled = true
  if (!er) return exit(0)
  if (!(er instanceof Error)) {
    log.error(er)
    return exit(1, true)
  }

  var m = er.code || er.message.match(/^(?:Error: )?(E[A-Z]+)/)
  if (m) {
    m = m[1]
    if (!constants[m] && !npm[m]) constants[m] = {}
    er.errno = npm[m] || constants[m]
  }

  console.error("")
  switch (er.code || er.errno) {
  case "ECONNREFUSED":
  case constants.ECONNREFUSED:
    log.error(er)
    log.error(["\nIf you are behind a proxy, please make sure that the"
              ,"'proxy' config is set properly.  See: 'npm help config'"
              ].join("\n"))
    break

  case "EACCES":
  case "EPERM":
  case constants.EACCES:
  case constants.EPERM:
    log.error(er)
    log.error(["\nPlease try running this command again as root/Administrator."
              ].join("\n"))
    break

  case npm.ELIFECYCLE:
    er.code = "ELIFECYCLE"
    log.error(er.message)
    log.error(["","Failed at the "+er.pkgid+" "+er.stage+" script."
              ,"This is most likely a problem with the "+er.pkgname+" package,"
              ,"not with npm itself."
              ,"Tell the author that this fails on your system:"
              ,"    "+er.script
              ,"You can get their info via:"
              ,"    npm owner ls "+er.pkgname
              ,"There is likely additional logging output above."
              ].join("\n"))
    break

  case npm.EJSONPARSE:
    er.code = "EJSONPARSE"
    log.error(er.message)
    log.error("File: "+er.file)
    log.error(["Failed to parse package.json data."
              ,"package.json must be actual JSON, not just JavaScript."
              ,"","This is not a bug in npm."
              ,"Tell the package author to fix their package.json file."
              ].join("\n"), "JSON.parse")
    break

  case npm.E404:
    er.code = "E404"
    if (er.pkgid && er.pkgid !== "-") {
      var msg = ["'"+er.pkgid+"' is not in the npm registry."
                ,"You should bug the author to publish it"]
      if (er.pkgid.match(/^node[\.\-]|[\.\-]js$/)) {
        var s = er.pkgid.replace(/^node[\.\-]|[\.\-]js$/g, "")
        if (s !== er.pkgid) {
          s = s.replace(/[^a-z0-9]/g, ' ')
          msg.push("\nMaybe try 'npm search " + s + "'")
        }
      }
      msg.push("\nNote that you can also install from a"
              ,"tarball, folder, or http url, or git url.")
      log.error(msg.join("\n"), "404")
    }
    break

  case npm.EPUBLISHCONFLICT:
    er.code = "EPUBLISHCONFLICT"
    log.error(["Cannot publish over existing version."
              ,"Bump the 'version' field, set the --force flag, or"
              ,"    npm unpublish '"+er.pkgid+"'"
              ,"and try again"
              ].join("\n"), "publish fail" )
    break

  case npm.EISGIT:
    er.code = "EISGIT"
    log.error([er.message
              ,"    "+er.path
              ,"Refusing to remove it. Update manually,"
              ,"or move it out of the way first."
              ].join("\n"), "git" )
    break

  case npm.ECYCLE:
    er.code = "ECYCLE"
    log.error([er.message
              ,"While installing: "+er.pkgid
              ,"Found a pathological dependency case that npm cannot solve."
              ,"Please report this to the package author."
              ].join("\n"))
    break

  case npm.ENOTSUP:
    er.code = "ENOTSUP"
    log.error([er.message
              ,"Not compatible with your version of node/npm: "+er.pkgid
              ,"Required: "+JSON.stringify(er.required)
              ,"Actual:   "
              +JSON.stringify({npm:npm.version
                              ,node:npm.config.get("node-version")})
              ].join("\n"))
    break

  case "EEXIST":
  case constants.EEXIST:
    log.error([er.message
              ,"File exists: "+er.path
              ,"Move it away, and try again."].join("\n"))
    break

  default:
    log.error(er)
    log.error(["You may report this log at:"
              ,"    <http://github.com/isaacs/npm/issues>"
              ,"or email it to:"
              ,"    <npm-@googlegroups.com>"
              ].join("\n"))
    break
  }

  var os = require("os")
  log.error("")
  log.error(os.type() + " " + os.release(), "System")
  log.error(process.argv
            .map(JSON.stringify).join(" "), "command")
  log.error(process.cwd(), "cwd")
  log.error(process.version, "node -v")
  log.error(npm.version, "npm -v")

  ; [ "file"
    , "path"
    , "type"
    , "syscall"
    , "fstream_path"
    , "fstream_unc_path"
    , "fstream_type"
    , "fstream_class"
    , "fstream_finish_call"
    , "fstream_linkpath"
    , "arguments"
    , "code"
    , "message"
    , "errno"
    ].forEach(function (k) {
      if (er[k]) log.error(er[k], k)
    })

  if (er.fstream_stack) {
    log.error(er.fstream_stack.join("\n"), "fstream_stack")
  }

  if (er.errno && typeof er.errno !== "object") log.error(er.errno, "errno")
  exit(typeof er.errno === "number" ? er.errno : 1)
}

function exit (code, noLog) {
  var doExit = npm.config.get("_exit")
  log.verbose([code, doExit], "exit")
  if (log.level === log.LEVEL.silent) noLog = true

  if (code && !noLog) writeLogFile(reallyExit)
  else rm("npm-debug.log", function () { rm(npm.tmp, reallyExit) })

  function reallyExit() {
    itWorked = !code
    //if (!itWorked) {
      if (!doExit) process.emit("exit", code)
      else process.exit(code)
    //}
  }
}

var writingLogFile = false
function writeLogFile (cb) {
  if (writingLogFile) return cb()
  writingLogFile = true
  wroteLogFile = true

  var fs = require("graceful-fs")
    , fstr = fs.createWriteStream("npm-debug.log")
    , util = require("util")

  log.history.forEach(function (m) {
    var lvl = log.LEVEL[m.level]
      , pref = m.pref ? " " + m.pref : ""
      , b = lvl + pref + " "
      , eol = process.platform === "win32" ? "\r\n" : "\n"
      , msg = typeof m.msg === "string" ? m.msg
            : msg instanceof Error ? msg.stack || msg.message
            : util.inspect(m.msg, 0, 4)
    fstr.write(new Buffer(b
                         +(msg.split(/\r?\n+/).join(eol+b))
                         + eol))
  })
  fstr.end()
  fstr.on("close", cb)
}
