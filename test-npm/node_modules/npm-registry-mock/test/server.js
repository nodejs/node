var assert = require("assert")
var mr = require("../index.js")

// clients
var request = require("request")
var RC = require("npm-registry-client")
var npm = require("npm")

// misc test helper
var mkdir = require("mkdirp")
var rimraf = require("rimraf")
var fs = require("fs")

// test-settings
var tempdir = __dirname + "/out"

var path = require('path')

// config
var port = 1331
var address = "http://localhost:" + port
var real = "http://registry.npmjs.org"
var conf = {
    cache: tempdir
  , registry: address }


beforeEach(function (done) {
  rimraf.sync(tempdir)
  mkdir.sync(tempdir)
  done()
})
afterEach(function (done) {
  rimraf.sync(tempdir)
  done()
})


describe("registry mocking - RegistryClient", function () {
  it("returns the underscore json", function (done) {
    mr(port, function (err, s) {
      if (err) return done(err)
      var client = new RC(conf)
      client.get("/underscore", function (er, data, raw, res) {
        assert.equal(data._id, "underscore")
        s.close()
        done(er)
      })
    })
  })
  it("responds to latest", function (done) {
    mr(port, function (err, s) {
      if (err) return done(err)
      var client = new RC(conf)
      client.get("/underscore/latest", function (er, data, raw, res) {
        assert.equal(data._id, "underscore@1.5.1")
        s.close()
        done(er)
      })
    })
  })
})

describe("registry mocking - npm.install", function () {
  var path = tempdir + "/node_modules/underscore/package.json"

  it("sends the module as tarball (version specified)", function (done) {
    mr({port: port}, function (err, s) {
      if (err) return done(err)
      npm.load({cache: tempdir, registry: address}, function () {
        npm.commands.install(tempdir, "underscore@1.3.1", function (err) {
          require.cache[path] = null
          var version = require(path).version
          assert.equal(version, "1.3.1")
          s.close()
          done()
        })
      })
    })
  })
  it("sends the module as tarball (no version specified -- latest)", function (done) {
    mr({port: port}, function (err, s) {
      if (err) return done(err)
      npm.load({cache: tempdir, registry: address}, function () {
        npm.commands.install(tempdir, "underscore", function (err) {
          require.cache[path] = null
          var version = require(path).version
          assert.equal(version, "1.5.1")
          s.close()
          done()
        })
      })
    })
  })
  it("i have a test package with one dependency", function (done) {
    mr({port: port}, function (err, s) {
      if (err) return done(err)
      npm.load({cache: tempdir, registry: address}, function () {
        npm.commands.install(tempdir, "test-package-with-one-dep", function (err) {
          var exists = fs.existsSync(tempdir + "/node_modules/test-package-with-one-dep/" +
            "node_modules/test-package/package.json")
          assert.ok(exists)
          s.close()
          done()
        })
      })
    })
  })
})

describe("extending the predefined mocks with custom ones", function () {
  it("handles new mocks", function (done) {
    var customMocks = {
      "get": {
        "/ente200": [200, {ente200: "true"}],
        "/ente400": [400, {ente400: "true"}],
        "/async/-/async-0.1.0.tgz": [200, __dirname + "/fixtures/async/-/async-0.1.0.tgz"],
        "/async/0.1.0": [200, __dirname + "/fixtures/async/0.1.0"],
      }
    }
    mr({port: port, mocks: customMocks}, function (err, s) {
      if (err) return done(err)
      request(address + "/ente200", function (er, res) {
        assert.equal(res.headers.connection, 'close')
        assert.deepEqual(res.body, JSON.stringify({ente200: "true"}))
        assert.equal(res.statusCode, 200)
        request(address + "/ente400", function (er, res) {
          assert.equal(res.body, JSON.stringify({ente400: "true"}))
          assert.equal(res.statusCode, 400)
          npm.load({cache: tempdir, registry: address}, function () {
            npm.commands.install(tempdir, "async@0.1.0", function (err) {
              var exists = fs.existsSync(tempdir + "/node_modules/async/package.json")
              assert.ok(exists)
              s.close()
              done()
            })
          })
        })
      })
    })
  })
  it("uses the fake registry (GH-22)", function (done) {
      var customMocks = {
        "get": {
          "/async/-/async-0.1.0.tgz": [200, __dirname + "/fixtures/async/-/async-0.1.0.tgz"],
          "/async/0.1.0": [200, __dirname + "/fixtures/async/0.1.0"],
        }
      }
      mr({port: port, mocks: customMocks}, function (err, s) {
        if (err) return done(err)
        var client = new RC(conf)
        client.get("/async/0.1.0", function (er, data, raw, res) {
          assert.notEqual(data.dist.tarball,
            "http://registry.npmjs.org/async/-/async-0.1.0.tgz")
          assert.ok(/localhost/.test(data.dist.tarball))
          s.close()
          done(er)
        })
      })
    })
  it("serves js-files", function (done) {
    var customMocks = {
      "get": {
        "/foo.js": [200, __dirname + "/fixtures/index.js"]
      }
    }

    var file = fs.readFileSync(__dirname + "/fixtures/index.js", "utf8")
    mr({port: port, mocks: customMocks}, function (err, s) {
      if (err) return done(err)
      request(address + "/foo.js", function (er, res) {
        assert.equal(res.body, file)
        s.close()
        done()
      })
    })
  })
  it("extends the custom mocks instead of overwriting", function (done) {
    var customMocks = {
      "get": {
        "/foo.js": [200, __dirname + "/fixtures/index.js"]
      }
    }

    var file = fs.readFileSync(__dirname + "/fixtures/index.js", "utf8")
    mr({port: port, mocks: customMocks}, function (err, s) {
      if (err) return done(err)
      request(address + "/foo.js", function (er, res) {
        assert.equal(res.body, file)
        var client = new RC(conf)
        client.get("/underscore/latest", function (er, data, raw, res) {
          assert.equal(data._id, "underscore@1.5.1")
          s.close()
          done(er)
        })
      })
    })
  })
  it("can hande custom data for js files", function (done) {
    var customMocks = {
      "get": {
        "/package.js": [200, {"ente" : true}],
        "/shrinkwrap.js": [200, {"ente" : true}]
      }
    }

    mr({port: port, mocks: customMocks}, function (err, s) {
      if (err) return done(err)
      request(address + "/package.js", function (er, res) {
        assert.equal(res.body, JSON.stringify({"ente" : true}))
        s.close()
        done()
      })
    })
  })
  it("overwrites the predefined routes, if custom one given", function (done) {
    var customMocks = {
      "get": {
        "/foo.js": [200, __dirname + "/fixtures/index.js"],
        "/underscore/1.3.1": [200, __dirname + "/fixtures/async/0.1.0"],
      }
    }

    var file = fs.readFileSync(__dirname + "/fixtures/index.js", "utf8")
    mr({port: port, mocks: customMocks}, function (err, s) {
      if (err) return done(err)
      request(address + "/foo.js", function (er, res) {
        assert.equal(res.body, file)
        var client = new RC(conf)
        client.get("/underscore/1.3.1", function (er, data, raw, res) {
          assert.equal(data._id, "async@0.1.0")
          s.close()
          done(er)
        })
      })
    })
  })
})

describe("injecting functions", function () {
  it("handles plugins", function (done) {
    function plugin (s) {
      s.get("/test").reply(500, {"foo": "true"})
      s.get("/test").reply(500, {"foo": "true"})
      s.get("/test").reply(200, {"lala": "true"})
    }
    mr({port: port, plugin: plugin}, function (err, s) {
      if (err) return done(err)
      request(address + "/test", function (er, res) {
        assert.deepEqual(res.body, JSON.stringify({foo: "true"}))
        assert.equal(res.statusCode, 500)
        request(address + "/test", function (er, res) {
          assert.deepEqual(res.body, JSON.stringify({foo: "true"}))
          assert.equal(res.statusCode, 500)
          request(address + "/test", function (er, res) {
            assert.deepEqual(res.body, JSON.stringify({lala: "true"}))
            assert.equal(res.statusCode, 200)
            s.close()
            done()
          })
        })
      })
    })
  })
  it("default routes still work", function (done) {
    function plugin (s) {
      s.get("/test").reply(500, {"foo": "true"})
      s.get("/test").reply(500, {"foo": "true"})
      s.get("/test").reply(200, {"lala": "true"})
    }
    mr({port: port, plugin: plugin}, function (err, s) {
      if (err) return done(err)
      var client = new RC(conf)
      client.get("/underscore/latest", function (er, data, raw, res) {
        assert.equal(data._id, "underscore@1.5.1")
        s.close()
        done(er)
      })
    })
  })
})

describe("api", function () {
  it("allows an options object with port but no mocks given", function (done) {
    mr({port: port}, function (err, s) {
      if (err) return done(err)
      var client = new RC(conf)
      client.get("/underscore/latest", function (er, data, raw, res) {
        assert.equal(data._id, "underscore@1.5.1")
        s.close()
        done(er)
      })
    })
  })
})

describe('multiple requests', function () {
  it('will not error after the first request, if nothing is specified', function (done) {
    mr({
      port: port
    }, function (err, s) {
      if (err) return done(err)
      var client = new RC(conf)
      client.get('/underscore/latest', function (er, data, raw, res) {
        assert.equal(er, null)
        client.get('/underscore/latest', function (er, data, raw, res) {
          assert.equal(er, null)
          client.get('/underscore/latest', function (er, data, raw, res) {
            s.close()
            done(er)
          })
        })
      })
    })
  })
})
