// XXX Todo:
// On windows, create a .cmd file.
// Read the #! in the file to see what it uses.  The vast majority
// of the time, this will be either:
// "#!/usr/bin/env <prog> <args...>"
// or:
// "#!<prog> <args...>"
//
// Write a binroot/pkg.bin + ".cmd" file that has this line in it:
// @<prog> <args...> %~dp0<target> %*

module.exports = cmdShim
cmdShim.ifExists = cmdShimIfExists

var fs = require("graceful-fs")
  , chain = require("slide").chain
  , mkdir = require("mkdirp")
  , rm = require("rimraf")
  , log = require("./log.js")
  , path = require("path")
  , relativize = require("./relativize.js")
  , npm = require("../npm.js")
  , shebangExpr = /^#\!\s*(?:\/usr\/bin\/env)?\s*([^ \t]+)(.*)$/

function cmdShimIfExists (from, to, cb) {
  fs.stat(from, function (er) {
    if (er) return cb()
    cmdShim(from, to, cb)
  })
}

function cmdShim (from, to, cb) {
  if (process.platform !== "win32") {
    return cb(new Error(".cmd shims only should be used on windows"))
  }

  chain
    ( [ [fs, "stat", from]
      , [rm, to + ".cmd"]
      , [rm, to]
      , [mkdir, path.dirname(to)]
      , [writeShim, from, to] ]
    , cb )
}

function writeShim (from, to, cb) {
  // make a cmd file and a sh script
  // First, check if the bin is a #! of some sort.
  // If not, then assume it's something that'll be compiled, or some other
  // sort of script, and just call it directly.
  fs.readFile(from, "utf8", function (er, data) {
    if (er) return writeShim_(from, to, null, null, cb)
    var firstLine = data.trim().split(/\r*\n/)[0]
      , shebang = firstLine.match(shebangExpr)
    if (!shebang) return writeShim_(from, to, null, null, cb)
    var prog = shebang[1]
      , args = shebang[2] || ""
    return writeShim_(from, to, prog, args, cb)
  })
}

function writeShim_ (from, to, prog, args, cb) {
  var shTarget = relativize(from, to)
    , target = shTarget.split("/").join("\\")
    , longProg
    , shProg = prog
    , shLongProg
  args = args || ""
  if (!prog) {
    prog = "\"%~dp0\\" + target + "\""
    shProg = "\"`dirname \"$0\"`/" + shTarget + "\""
    args = ""
    target = ""
    shTarget = ""
  } else {
    longProg = "\"%~dp0\\" + prog + ".exe\""
    shLongProg = "\"`dirname \"$0\"`/" + prog + "\""
    target = "\"%~dp0\\" + target + "\""
    shTarget = "\"`dirname \"$0\"`/" + shTarget + "\""
  }

  // @IF EXIST "%~dp0\node.exe" (
  //   "%~dp0\node.exe" "%~dp0\.\node_modules\npm\bin\npm-cli.js" %*
  // ) ELSE (
  //   node "%~dp0\.\node_modules\npm\bin\npm-cli.js" %*
  // )
  var cmd
  if (longProg) {
    cmd = "@IF EXIST " + longProg + " (\r\n"
        + "  " + longProg + " " + args + " " + target + " %*\r\n"
        + ") ELSE (\r\n"
        + "  " + prog + " " + args + " " + target + " %*\r\n"
        + ")"
  } else {
    cmd = prog + " " + args + " " + target + " %*\r\n"
  }

  cmd = ":: Created by npm, please don't edit manually.\r\n" + cmd

  // #!/bin/sh
  // if [ -x "`dirname "$0"`/node.exe" ]; then
  //   "`dirname "$0"`/node.exe" "`dirname "$0"`/node_modules/npm/bin/npm-cli.js" "$@"
  // else
  //   node "`dirname "$0"`/node_modules/npm/bin/npm-cli.js" "$@"
  // fi
  var sh = "#!/bin/sh\n"

  if (shLongProg) {
    sh = sh
       + "if [ -x "+shLongProg+" ]; then\n"
       + "  " + shLongProg + " " + args + " " + shTarget + " \"$@\"\n"
       + "  ret=$?\n"
       + "else \n"
       + "  " + shProg + " " + args + " " + shTarget + " \"$@\"\n"
       + "  ret=$?\n"
       + "fi\n"
       + "exit $ret\n"
  } else {
    sh = shProg + " " + args + " " + shTarget + " \"$@\"\n"
       + "exit $?\n"
  }

  fs.writeFile(to + ".cmd", cmd, "utf8", function (er) {
    if (er) {
      log.warn("Could not write "+to+".cmd", "cmdShim")
      return cb(er)
    }
    fs.writeFile(to, sh, "utf8", function (er) {
      if (er) {
        log.warn("Could not write "+to, "shShim")
        return cb(er)
      }
      fs.chmod(to, 0755, cb)
    })
  })
}
