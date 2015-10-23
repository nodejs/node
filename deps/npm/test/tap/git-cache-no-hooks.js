var test = require("tap").test
  , fs = require("fs")
  , path = require("path")
  , rimraf = require("rimraf")
  , mkdirp = require("mkdirp")
  , spawn = require("child_process").spawn
  , npmCli = require.resolve("../../bin/npm-cli.js")
  , node = process.execPath
  , pkg = path.resolve(__dirname, "git-cache-no-hooks")
  , tmp = path.join(pkg, "tmp")
  , cache = path.join(pkg, "cache")


test("setup", function (t) {
  rimraf.sync(pkg)
  mkdirp.sync(pkg)
  mkdirp.sync(cache)
  mkdirp.sync(tmp)
  mkdirp.sync(path.resolve(pkg, "node_modules"))
  t.end()
})

test("git-cache-no-hooks: install a git dependency", function (t) {

  // disable git integration tests on Travis.
  if (process.env.TRAVIS) return t.end()

  var command = [ npmCli
                , "install"
                , "git://github.com/nigelzor/npm-4503-a.git"
                ]
  var child = spawn(node, command, {
    cwd: pkg,
    env: {
      "npm_config_cache"  : cache,
      "npm_config_tmp"    : tmp,
      "npm_config_prefix" : pkg,
      "npm_config_global" : "false",
      "npm_config_umask"  : "00",
      HOME                : process.env.HOME,
      Path                : process.env.PATH,
      PATH                : process.env.PATH
    },
    stdio: "inherit"
  })

  child.on("close", function (code) {
    t.equal(code, 0, "npm install should succeed")

    // verify permissions on git hooks
    var repoDir = "git-github-com-nigelzor-npm-4503-a-git-40c5cb24"
    var hooksPath = path.join(cache, "_git-remotes", repoDir, "hooks")
    fs.readdir(hooksPath, function (err) {
      t.equal(err && err.code, "ENOENT", "hooks are not brought along with repo")
      t.end()
    })
  })
})

test("cleanup", function (t) {
  rimraf.sync(pkg)
  t.end()
})
