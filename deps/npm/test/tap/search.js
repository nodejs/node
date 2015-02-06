var common = require("../common-tap.js")
var test = require("tap").test
var rimraf = require("rimraf")
var mr = require("npm-registry-mock")
var fs = require("fs")
var path = require("path")
var pkg = path.resolve(__dirname, "search")
var cache = path.resolve(pkg, "cache")
var registryCache = path.resolve(cache, "localhost_1337", "-", "all")
var cacheJsonFile = path.resolve(registryCache, ".cache.json")
var mkdirp = require("mkdirp")

var timeMock = {
  epoch: 1411727900,
  future: 1411727900+100,
  all: 1411727900+25,
  since: 0  // filled by since server callback
}

var EXEC_OPTS = {}

function cleanupCache() {
  rimraf.sync(cache)
}
function cleanup () { cleanupCache() }

function setupCache() {
  mkdirp.sync(cache)
  mkdirp.sync(registryCache)
  var res = fs.writeFileSync(cacheJsonFile, stringifyUpdated(timeMock.epoch))
  if (res) throw new Error("Creating cache file failed")
}

var mocks = {
  /* Since request, always response with an _update time > the time requested */
  sinceFuture: function(server) {
    server.filteringPathRegEx(/startkey=[^&]*/g, function(s) {
      var _allMock = JSON.parse(JSON.stringify(allMock))
      timeMock.since = _allMock._updated = s.replace("startkey=", "")
      server.get("/-/all/since?stale=update_after&" + s)
        .reply(200, _allMock)
      return s
    })
  },
  allFutureUpdatedOnly: function(server) {
    server.get("/-/all")
      .reply(200, stringifyUpdated(timeMock.future))
  },
  all: function(server) {
    server.get("/-/all")
      .reply(200, allMock)
  }
}


test("No previous cache, init cache triggered by first search", function(t) {
  cleanupCache()

  mr({ port: common.port, plugin: mocks.allFutureUpdatedOnly }, function (err, s) {
    common.npm([
      "search", "do not do extra search work on my behalf",
      "--registry", common.registry,
      "--cache", cache,
      "--loglevel", "silent",
      "--color", "always"
    ],
    EXEC_OPTS,
    function(err, code) {
      s.close()
      t.equal(code, 0, "search finished successfully")
      t.ifErr(err, "search finished successfully")

      t.ok(fs.existsSync(cacheJsonFile),
        cacheJsonFile + " expected to have been created")
      var cacheData = JSON.parse(fs.readFileSync(cacheJsonFile, "utf8"))
      t.equal(cacheData._updated, String(timeMock.future))
      t.end()
    })
  })
})

test("previous cache, _updated set, should trigger since request", function(t) {
  cleanupCache()
  setupCache()

  function m(server) {
    [ mocks.all, mocks.sinceFuture ].forEach(function(m) {
      m(server)
    })
  }
  mr({ port: common.port, plugin: m }, function (err, s) {
    common.npm([
      "search", "do not do extra search work on my behalf",
      "--registry", common.registry,
      "--cache", cache,
      "--loglevel", "silly",
      "--color", "always"
    ],
    EXEC_OPTS,
    function(err, code) {
      s.close()
      t.equal(code, 0, "search finished successfully")
      t.ifErr(err, "search finished successfully")

      var cacheData = JSON.parse(fs.readFileSync(cacheJsonFile, "utf8"))
      t.equal(cacheData._updated,
        timeMock.since,
        "cache update time gotten from since response")
      cleanupCache()
      t.end()
    })
  })
})


var searches = [
  {
    term: "f36b6a6123da50959741e2ce4d634f96ec668c56",
    description: "non-regex",
    location: 241
  },
  {
    term: "/f36b6a6123da50959741e2ce4d634f96ec668c56/",
    description: "regex",
    location: 241
  }
]

searches.forEach(function(search) {
  test(search.description + " search in color", function(t) {
    cleanupCache()
    mr({ port: common.port, plugin: mocks.all }, function (er, s) {
      common.npm([
        "search", search.term,
        "--registry", common.registry,
        "--cache", cache,
        "--loglevel", "silent",
        "--color", "always"
      ],
      EXEC_OPTS,
      function(err, code, stdout) {
        s.close()
        t.equal(code, 0, "search finished successfully")
        t.ifErr(err, "search finished successfully")
        // \033 == \u001B
        var markStart = "\u001B\\[[0-9][0-9]m"
        var markEnd = "\u001B\\[0m"

        var re = new RegExp(markStart + ".*?" + markEnd)

        var cnt = stdout.search(re)
        t.equal(cnt, search.location,
          search.description + " search for " + search.term)
        t.end()
      })
    })
  })
})

test("cleanup", function (t) {
  cleanup()
  t.end()
})

function stringifyUpdated(time) {
  return JSON.stringify({ _updated : String(time) })
}

var allMock = {
  "_updated": timeMock.all,
  "generator-frontcow": {
      "name": "generator-frontcow",
      "description": "f36b6a6123da50959741e2ce4d634f96ec668c56 This is a fake description to ensure we do not accidentally search the real npm registry or use some kind of cache",
      "dist-tags": {
          "latest": "0.1.19"
      },
      "maintainers": [
          {
              "name": "bcabanes",
              "email": "contact@benjamincabanes.com"
          }
      ],
      "homepage": "https://github.com/bcabanes/generator-frontcow",
      "keywords": [
          "sass",
          "frontend",
          "yeoman-generator",
          "atomic",
          "design",
          "sass",
          "foundation",
          "foundation5",
          "atomic design",
          "bourbon",
          "polyfill",
          "font awesome"
      ],
      "repository": {
          "type": "git",
          "url": "https://github.com/bcabanes/generator-frontcow"
      },
      "author": {
          "name": "ben",
          "email": "contact@benjamincabanes.com",
          "url": "https://github.com/bcabanes"
      },
      "bugs": {
          "url": "https://github.com/bcabanes/generator-frontcow/issues"
      },
      "license": "MIT",
      "readmeFilename": "README.md",
      "time": {
          "modified": "2014-10-03T02:26:18.406Z"
      },
      "versions": {
          "0.1.19": "latest"
      }
  },
  "marko": {
      "name": "marko",
      "description": "Marko is an extensible, streaming, asynchronous, high performance, HTML-based templating language that can be used in Node.js or in the browser.",
      "dist-tags": {
          "latest": "1.2.16"
      },
      "maintainers": [
          {
              "name": "pnidem",
              "email": "pnidem@gmail.com"
          },
          {
              "name": "philidem",
              "email": "phillip.idem@gmail.com"
          }
      ],
      "homepage": "https://github.com/raptorjs/marko",
      "keywords": [
          "templating",
          "template",
          "async",
          "streaming"
      ],
      "repository": {
          "type": "git",
          "url": "https://github.com/raptorjs/marko.git"
      },
      "author": {
          "name": "Patrick Steele-Idem",
          "email": "pnidem@gmail.com"
      },
      "bugs": {
          "url": "https://github.com/raptorjs/marko/issues"
      },
      "license": "Apache License v2.0",
      "readmeFilename": "README.md",
      "users": {
          "pnidem": true
      },
      "time": {
          "modified": "2014-10-03T02:27:31.775Z"
      },
      "versions": {
          "1.2.16": "latest"
      }
  }
}
