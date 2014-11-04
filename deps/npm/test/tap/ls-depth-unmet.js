var common = require("../common-tap")
  , test = require("tap").test
  , path = require("path")
  , rimraf = require("rimraf")
  , osenv = require("osenv")
  , mkdirp = require("mkdirp")
  , pkg = path.resolve(__dirname, "ls-depth-unmet")
  , mr = require("npm-registry-mock")
  , opts = {cwd: pkg}
  , cache = path.resolve(pkg, "cache")
  , tmp = path.resolve(pkg, "tmp")
  , nodeModules = path.resolve(pkg, "node_modules")


function cleanup () {
  process.chdir(osenv.tmpdir())
  rimraf.sync(cache)
  rimraf.sync(tmp)
  rimraf.sync(nodeModules)
}

test("setup", function (t) {
  cleanup()
  mkdirp.sync(cache)
  mkdirp.sync(tmp)
  mr(common.port, function (s) {
    var cmd = ["install", "underscore@1.3.1", "mkdirp", "test-package-with-one-dep", "--registry=" + common.registry]
    common.npm(cmd, opts, function (er, c) {
      if (er) throw er
      t.equal(c, 0)
      s.close()
      t.end()
    })
  })
})

test("npm ls --depth=0", function (t) {
  common.npm(["ls", "--depth=0"], opts, function (er, c, out) {
    if (er) throw er
    t.equal(c, 1)
    t.has(out, /UNMET DEPENDENCY optimist@0\.6\.0/
      , "output contains optimist@0.6.0 and labeled as unmet dependency")
    t.has(out, /mkdirp@0\.3\.5 extraneous/
      , "output contains mkdirp@0.3.5 and labeled as extraneous")
    t.has(out, /underscore@1\.3\.1 invalid/
      , "output contains underscore@1.3.1 and labeled as invalid")
    t.has(out, /test-package-with-one-dep@0\.0\.0\n/
      , "output contains test-package-with-one-dep@0.0.0 and has no labels")
    t.doesNotHave(out, /test-package@0\.0\.0/
      , "output does not contain test-package@0.0.0 which is at depth=1")
    t.end()
  })
})

test("npm ls --depth=1", function (t) {
  common.npm(["ls", "--depth=1"], opts, function (er, c, out) {
    if (er) throw er
    t.equal(c, 1)
    t.has(out, /UNMET DEPENDENCY optimist@0\.6\.0/
      , "output contains optimist@0.6.0 and labeled as unmet dependency")
    t.has(out, /mkdirp@0\.3\.5 extraneous/
      , "output contains mkdirp@0.3.5 and labeled as extraneous")
    t.has(out, /underscore@1\.3\.1 invalid/
      , "output contains underscore@1.3.1 and labeled as invalid")
    t.has(out, /test-package-with-one-dep@0\.0\.0\n/
      , "output contains test-package-with-one-dep@0.0.0 and has no labels")
    t.has(out, /test-package@0\.0\.0/
      , "output contains test-package@0.0.0 which is at depth=1")
    t.end()
  })
})

test("npm ls --depth=Infinity", function (t) {
  // travis has a preconfigured depth=0, in general we can not depend
  // on the default value in all environments, so explictly set it here
  common.npm(["ls", "--depth=Infinity"], opts, function (er, c, out) {
    if (er) throw er
    t.equal(c, 1)
    t.has(out, /UNMET DEPENDENCY optimist@0\.6\.0/
      , "output contains optimist@0.6.0 and labeled as unmet dependency")
    t.has(out, /mkdirp@0\.3\.5 extraneous/
      , "output contains mkdirp@0.3.5 and labeled as extraneous")
    t.has(out, /underscore@1\.3\.1 invalid/
      , "output contains underscore@1.3.1 and labeled as invalid")
    t.has(out, /test-package-with-one-dep@0\.0\.0\n/
      , "output contains test-package-with-one-dep@0.0.0 and has no labels")
    t.has(out, /test-package@0\.0\.0/
      , "output contains test-package@0.0.0 which is at depth=1")
    t.end()
  })
})

test("cleanup", function (t) {
  cleanup()
  t.end()
})
