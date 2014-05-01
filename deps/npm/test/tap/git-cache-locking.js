var test = require("tap").test
  , path = require("path")
  , rimraf = require("rimraf")
  , mkdirp = require("mkdirp")
  , spawn = require("child_process").spawn
  , npm = require.resolve("../../bin/npm-cli.js")
  , node = process.execPath
  , pkg = path.resolve(__dirname, "git-cache-locking")
  , tmp = path.join(pkg, "tmp")
  , cache = path.join(pkg, "cache")


test("setup", function (t) {
  rimraf.sync(pkg)
  mkdirp.sync(pkg)
  mkdirp.sync(cache)
  mkdirp.sync(tmp)
  mkdirp.sync(path.resolve(pkg, 'node_modules'))
  t.end()
})

test("git-cache-locking: install a git dependency", function (t) {

  // disable git integration tests on Travis.
  if (process.env.TRAVIS) return t.end()

  // package c depends on a.git#master and b.git#master
  // package b depends on a.git#master
  var child = spawn(node, [npm, "install", "git://github.com/nigelzor/npm-4503-c.git"], {
    cwd: pkg,
    env: {
      npm_config_cache: cache,
      npm_config_tmp: tmp,
      npm_config_prefix: pkg,
      npm_config_global: "false",
      HOME: process.env.HOME,
      Path: process.env.PATH,
      PATH: process.env.PATH
    },
    stdio: "inherit"
  })

  child.on("close", function (code) {
    t.equal(0, code, "npm install should succeed")
    t.end()
  })
})

test('cleanup', function(t) {
  rimraf.sync(pkg)
  t.end()
})
