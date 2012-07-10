module.exports = exec
exec.spawn = spawn

var log = require("npmlog")
  , child_process = require("child_process")
  , util = require("util")
  , npm = require("../npm.js")
  , myUID = process.getuid ? process.getuid() : null
  , myGID = process.getgid ? process.getgid() : null
  , isRoot = process.getuid && myUID === 0
  , constants = require("constants")
  , uidNumber = require("uid-number")

function exec (cmd, args, env, takeOver, cwd, uid, gid, cb) {
  if (typeof cb !== "function") cb = gid, gid = null
  if (typeof cb !== "function") cb = uid, uid = null
  if (typeof cb !== "function") cb = cwd, cwd = null
  if (typeof cb !== "function") cb = takeOver, takeOver = true
  if (typeof cb !== "function") cb = env, env = process.env
  gid = gid == null ? myGID : gid
  uid = uid == null ? myUID : uid
  if (!isRoot) {
    if (npm.config.get("unsafe-perm")) {
      uid = myUID
      gid = myGID
    } else if (uid !== myUID || gid !== myGID) {
      var e = new Error("EPERM: setuid() operation not permitted")
      e.errno = constants.EPERM
      return cb(e)
    }
  }
  if (uid !== myUID) {
    log.verbose("set uid", "from=%s to=%s", myUID, uid)
  }

  if (uid && gid && (isNaN(uid) || isNaN(gid))) {
    // get the numeric values
    return uidNumber(uid, gid, function (er, uid, gid) {
      if (er) return cb(er)
      exec(cmd, args, env, takeOver, cwd, uid, gid, cb)
    })
  }

  log.silly("exec", cmd+" "+args.map(JSON.stringify).join(" "))
  var stdout = ""
    , stderr = ""
    , cp = spawn(cmd, args, env, takeOver, cwd, uid, gid)
  cp.stdout && cp.stdout.on("data", function (chunk) {
    if (chunk) stdout += chunk
  })
  cp.stderr && cp.stderr.on("data", function (chunk) {
    if (chunk) stderr += chunk
  })
  cp.on("exit", function (code) {
    var er = null
    if (code) er = new Error("`"+cmd
                            +(args.length ? " "
                                          + args.map(JSON.stringify).join(" ")
                                          : "")
                            +"` failed with "+code)
    cb(er, code, stdout, stderr)
  })
  return cp
}


function spawn (c, a, env, takeOver, cwd, uid, gid) {
  var fds = [ 0, 1, 2 ]
    , opts = { customFds : takeOver ? fds : [-1,-1,-1]
             , env : env || process.env
             , cwd : cwd || null }
    , cp

  if (uid && !isNaN(uid)) opts.uid = +uid
  if (gid && !isNaN(gid)) opts.gid = +gid

  var name = c +" "+ a.map(JSON.stringify).join(" ")
  log.silly([c, a, opts.cwd], "spawning")
  cp = child_process.spawn(c, a, opts)
  cp.name = name
  return cp
}
