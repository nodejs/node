var fs = require("fs")
var resolve = require("path").resolve

var chain = require("slide").chain
var osenv = require("osenv")
var mkdirp = require("mkdirp")
var rimraf = require("rimraf")
var test = require("tap").test

var npm = require("../../lib/npm.js")
var common = require("../common-tap.js")

var pkg = resolve(__dirname, "add-remote-git")
var repo = resolve(__dirname, "add-remote-git-repo")

var daemon
var daemonPID
var git

test("setup", function (t) {
  bootstrap()
  setup(function (er, r) {
    if (er) {
      throw er
    }

    if (!er) {
      daemon = r[r.length - 2]
      daemonPID = r[r.length - 1]
    }

    t.end()
  })
})

test("install from repo on 'Windows'", function (t) {
  // before we confuse everything by switching the platform
  require("../../lib/install.js")
  require("../../lib/unbuild.js")
  process.platform = "win32"
  process.chdir(pkg)
  npm.commands.install(".", [], function (er) {
    t.ifError(er, "npm installed via git")

    t.end()
  })
})

test("clean", function (t) {
  daemon.on("close", function () {
    cleanup()
    t.end()
  })
  process.kill(daemonPID)
})

var pjParent = JSON.stringify({
  name : "parent",
  version : "1.2.3",
  dependencies : {
    "child" : "git://localhost:1234/child.git"
  }
}, null, 2) + "\n"

var pjChild = JSON.stringify({
  name : "child",
  version : "1.0.3"
}, null, 2) + "\n"

function bootstrap () {
  rimraf.sync(pkg)
  mkdirp.sync(pkg)
  fs.writeFileSync(resolve(pkg, "package.json"), pjParent)
}

function setup (cb) {
  rimraf.sync(repo)
  mkdirp.sync(repo)
  fs.writeFileSync(resolve(repo, "package.json"), pjChild)
  npm.load({ registry : common.registry, loglevel : "silent" }, function () {
    // some really cheesy monkeypatching
    require("module")._cache[require.resolve("which")] = {
      exports : function (_, cb) { cb() }
    }
    git = require("../../lib/utils/git.js")

    function startDaemon (cb) {
      // start git server
      var d = git.spawn(
        [
          "daemon",
          "--verbose",
          "--listen=localhost",
          "--export-all",
          "--base-path=.",
          "--port=1234"
        ],
        {
          cwd : pkg,
          env : process.env,
          stdio : ["pipe", "pipe", "pipe"]
        }
      )
      d.stderr.on("data", childFinder)

      function childFinder (c) {
        var cpid = c.toString().match(/^\[(\d+)\]/)
        if (cpid[1]) {
          this.removeListener("data", childFinder)
          cb(null, [d, cpid[1]])
        }
      }
    }

    var opts = {
      cwd : repo,
      env : process.env
    }

    chain(
      [
        git.chainableExec(["init"], opts),
        git.chainableExec(["config", "user.name", "PhantomFaker"], opts),
        git.chainableExec(["config", "user.email", "nope@not.real"], opts),
        git.chainableExec(["add", "package.json"], opts),
        git.chainableExec(["commit", "-m", "stub package"], opts),
        git.chainableExec(
          ["clone", "--bare", repo, "child.git"],
          { cwd : pkg, env : process.env }
        ),
        startDaemon
      ],
      cb
    )
  })
}

function cleanup () {
  process.chdir(osenv.tmpdir())
  rimraf.sync(repo)
  rimraf.sync(pkg)
}
