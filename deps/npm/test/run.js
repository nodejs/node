// Everything in this file uses child processes, because we're
// testing a command line utility.

var chain = require("slide").chain
var child_process = require("child_process")
var path = require("path")
  , testdir = __dirname
  , fs = require("graceful-fs")
  , npmpkg = path.dirname(testdir)
  , npmcli = path.resolve(npmpkg, "bin", "npm-cli.js")

var temp = process.env.TMPDIR
         || process.env.TMP
         || process.env.TEMP
         || ( process.platform === "win32"
            ? "c:\\windows\\temp"
            : "/tmp" )

temp = path.resolve(temp, "npm-test-" + process.pid)

var root = path.resolve(temp, "root")
  , cache = path.resolve(temp, "npm_cache")

var failures = 0
  , mkdir = require("mkdirp")
  , rimraf = require("rimraf")

var pathEnvSplit = process.platform === "win32" ? ";" : ":"
  , pathEnv = process.env.PATH.split(pathEnvSplit)
  , npmPath = process.platform === "win32" ? root : path.join(root, "bin")

pathEnv.unshift(npmPath, path.join(root, "node_modules", ".bin"))

// lastly, make sure that we get the same node that is being used to do
// run this script.  That's very important, especially when running this
// test file from in the node source folder.
pathEnv.unshift(path.dirname(process.execPath))

// the env for all the test installs etc.
var env = {}
Object.keys(process.env).forEach(function (i) {
  env[i] = process.env[i]
})
env.npm_config_prefix = root
env.npm_config_color = "always"
env.npm_config_global = "true"
// have to set this to false, or it'll try to test itself forever
env.npm_config_npat = "false"
env.PATH = pathEnv.join(pathEnvSplit)
env.NODE_PATH = path.join(root, "node_modules")
env.npm_config_cache = cache



function cleanup (cb) {
  if (failures !== 0) return
  rimraf(root, function (er) {
    if (er) cb(er)
    mkdir(root, 0755, cb)
  })
}

function prefix (content, pref) {
  return pref + (content.trim().split(/\r?\n/).join("\n" + pref))
}

var execCount = 0
function exec (cmd, cwd, shouldFail, cb) {
  if (typeof shouldFail === "function") {
    cb = shouldFail, shouldFail = false
  }
  console.error("\n+"+cmd + (shouldFail ? " (expect failure)" : ""))

  // special: replace 'node' with the current execPath,
  // and 'npm' with the thing we installed.
  var cmdShow = cmd
  var npmReplace = path.resolve(npmPath, "npm")
  var nodeReplace = process.execPath
  if (process.platform === "win32") {
    npmReplace = '"' + npmReplace + '"'
    nodeReplace = '"' + nodeReplace + '"'
  }
  cmd = cmd.replace(/^npm /, npmReplace + " ")
  cmd = cmd.replace(/^node /, nodeReplace + " ")

  console.error("$$$$$$ cd %s; PATH=%s %s", cwd, env.PATH, cmd)

  child_process.exec(cmd, {cwd: cwd, env: env}, function (er, stdout, stderr) {
    console.error("$$$$$$ after command", cmd, cwd)
    if (stdout) {
      console.error(prefix(stdout, " 1> "))
    }
    if (stderr) {
      console.error(prefix(stderr, " 2> "))
    }

    execCount ++
    if (!shouldFail && !er || shouldFail && er) {
      // stdout = (""+stdout).trim()
      console.log("ok " + execCount + " " + cmdShow)
      return cb()
    } else {
      console.log("not ok " + execCount + " " + cmdShow)
      cb(new Error("failed "+cmdShow))
    }
  })
}

function execChain (cmds, cb) {
  chain(cmds.map(function (args) {
    return [exec].concat(args)
  }), cb)
}

function flatten (arr) {
  return arr.reduce(function (l, r) {
    return l.concat(r)
  }, [])
}

function setup (cb) {
  cleanup(function (er) {
    if (er) return cb(er)
    execChain([ [ "node \""+npmcli+"\" install \""+npmpkg+"\"", root ],
                [ "npm config set package-config:foo boo",  root ]
              ], cb)
  })
}

function main (cb) {
  console.log("# testing in %s", temp)
  console.log("# global prefix = %s", root)



  failures = 0

  process.chdir(testdir)
  var base = path.resolve(root, path.join("lib", "node_modules"))

  // get the list of packages
  var packages = fs.readdirSync(path.resolve(testdir, "packages"))
  packages = packages.filter(function (p) {
    return p && !p.match(/^\./)
  })

  installAllThenTestAll()

  function installAllThenTestAll () {
    var packagesToRm = packages.slice(0)
    if (process.platform !== "win32") {
      // Windows can't handle npm rm npm due to file-in-use issues.
      packagesToRm.push("npm")
    }

    chain(
        [ setup
        , [ exec, "npm install "+npmpkg, testdir ]
        , [ execChain, packages.map(function (p) {
              return [ "npm install packages/"+p, testdir ]
            }) ]
        , [ execChain, packages.map(function (p) {
              return [ "npm test -ddd", path.resolve(base, p) ]
            }) ]
        , [ execChain, packagesToRm.map(function (p) {
              return [ "npm rm "+p, root ]
            }) ]
        , installAndTestEach
        ]
      , cb
      )
  }

  function installAndTestEach (cb) {
    var thingsToChain = [
        setup
        , [ execChain, flatten(packages.map(function (p) {
              return [ [ "npm install packages/"+p, testdir ]
                     , [ "npm test", path.resolve(base, p) ]
                     , [ "npm rm "+p, root ] ]
            })) ]
      ]
    if (process.platform !== "win32") {
      // Windows can't handle npm rm npm due to file-in-use issues.
      thingsToChain.push([exec, "npm rm npm", testdir])
    }

    chain(thingsToChain, cb)
  }
}

main(function (er) {
  console.log("1.." + execCount)
  if (er) throw er
})
