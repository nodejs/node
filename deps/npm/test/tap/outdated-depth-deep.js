var common = require("../common-tap")
  , path = require("path")
  , test = require("tap").test
  , rimraf = require("rimraf")
  , npm = require("../../")
  , mr = require("npm-registry-mock")
  , pkg = path.resolve(__dirname, "outdated-depth-deep")
  , cache = path.resolve(pkg, "cache")

var osenv = require("osenv")
var mkdirp = require("mkdirp")
var fs = require("fs")

var pj = JSON.stringify({
  "name": "whatever",
  "description": "yeah idk",
  "version": "1.2.3",
  "main": "index.js",
  "dependencies": {
    "underscore": "1.3.1",
    "npm-test-peer-deps": "0.0.0"
  },
  "repository": "git://github.com/luk-/whatever"
}, null, 2)

function cleanup () {
  process.chdir(osenv.tmpdir())
  rimraf.sync(pkg)
}

function setup () {
  mkdirp.sync(pkg)
  process.chdir(pkg)
  fs.writeFileSync(path.resolve(pkg, "package.json"), pj)
}

test("setup", function (t) {
  cleanup()
  setup()
  t.end()
})

test("outdated depth deep (9999)", function (t) {
  var underscoreOutdated = ["underscore", "1.3.1", "1.3.1", "1.5.1", "1.3.1"]
  var childPkg = path.resolve(pkg, "node_modules", "npm-test-peer-deps")

  var expected = [ [pkg].concat(underscoreOutdated),
                   [childPkg].concat(underscoreOutdated) ]

  process.chdir(pkg)

  mr({port : common.port}, function (er, s) {
    npm.load({
      cache: cache
    , loglevel: "silent"
    , registry: common.registry
    , depth: 9999
    }
    , function () {
        npm.install(".", function (er) {
          if (er) throw new Error(er)
          npm.outdated(function (err, d) {
            if (err) throw new Error(err)
            t.deepEqual(d, expected)
            s.close()
            t.end()
          })
        })
      }
    )
  })
})


test("cleanup", function (t) {
  cleanup()
  t.end()
})
