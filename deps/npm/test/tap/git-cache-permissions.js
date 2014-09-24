var test = require("tap").test
  , fs = require("fs")
  , path = require("path")
  , rimraf = require("rimraf")
  , mkdirp = require("mkdirp")
  , spawn = require("child_process").spawn
  , npm = require("../../lib/npm")
  , npmCli = require.resolve("../../bin/npm-cli.js")
  , node = process.execPath
  , pkg = path.resolve(__dirname, "git-cache-permissions")
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

test("git-cache-permissions: install a git dependency", function (t) {

  // disable git integration tests on Travis.
  if (process.env.TRAVIS) return t.end()

  var command = [ npmCli
                , "install"
                , "git://github.com/nigelzor/npm-4503-a.git"
                ]
  var child = spawn(node, command, {
    cwd: pkg,
    env: {
      npm_config_cache: cache,
      npm_config_tmp: tmp,
      npm_config_prefix: pkg,
      npm_config_global: "false",
      npm_config_umask: "00",
      HOME: process.env.HOME,
      Path: process.env.PATH,
      PATH: process.env.PATH
    },
    stdio: "inherit"
  })

  child.on("close", function (code) {
    t.equal(code, 0, "npm install should succeed")

    // verify permissions on git hooks
    var repoDir = "git-github-com-nigelzor-npm-4503-a-git-40c5cb24"
    var hooksPath = path.join(cache, "_git-remotes", repoDir, "hooks")
    fs.readdir(hooksPath, function (err, files) {
      if (err) {
        t.ok(false, "error reading hooks: " + err)
        t.end()
      }

      files.forEach(function (file) {
        var stats = fs.statSync(path.join(hooksPath, file))
        var message = "hook [" + file + "] should have correct permissions"

        // Possible error conditions and the resulting file modes on hooks
        // npm.modes.file is used directly -> "100666"
        // permissions are left untouched  -> "100755"
        // we do not want permissions left untouched because of
        // https://github.com/npm/npm/issues/3117
        t.equal(stats.mode.toString(8), "100777", message)
      })

      t.end()
    })
  })
})

test('cleanup', function(t) {
  rimraf.sync(pkg)
  t.end()
})
