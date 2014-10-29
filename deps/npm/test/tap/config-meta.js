// this is a weird meta test.  It verifies that all the instances of
// `npm.config.get(...)` are:
// a) Simple strings, and not variables
// b) Documented
// c) Defined in the `npmconf` package.

var test = require("tap").test
var fs = require("fs")
var path = require("path")
var root = path.resolve(__dirname, "..", "..")
var lib = path.resolve(root, "lib")
var nm = path.resolve(root, "node_modules")
var doc = path.resolve(root, "doc/misc/npm-config.md")
var FILES = []
var CONFS = {}
var DOC = {}

var exceptions = [
  path.resolve(lib, "adduser.js"),
  path.resolve(lib, "config.js"),
  path.resolve(lib, "publish.js"),
  path.resolve(lib, "utils", "lifecycle.js"),
  path.resolve(lib, "utils", "map-to-registry.js"),
  path.resolve(nm, "npm-registry-client", "lib", "publish.js"),
  path.resolve(nm, "npm-registry-client", "lib", "request.js")
]

test("get files", function (t) {
  walk(nm)
  walk(lib)
  t.pass("got files")
  t.end()

  function walk(lib) {
    var files = fs.readdirSync(lib).map(function (f) {
      return path.resolve(lib, f)
    })
    files.forEach(function (f) {
      try {
        var s = fs.statSync(f)
      } catch (er) {
        return
      }
      if (s.isDirectory())
        walk(f)
      else if (f.match(/\.js$/))
        FILES.push(f)
    })
  }
})

test("get lines", function (t) {
  FILES.forEach(function (f) {
    var lines = fs.readFileSync(f, 'utf8').split(/\r|\n/)
    lines.forEach(function (l, i) {
      var matches = l.split(/conf(?:ig)?\.get\(/g)
      matches.shift()
      matches.forEach(function (m) {
        m = m.split(')').shift()
        var literal = m.match(/^['"].+['"]$/)
        if (literal) {
          m = m.slice(1, -1)
          if (!m.match(/^\_/) && m !== 'argv')
            CONFS[m] = {
              file: f,
              line: i
            }
        } else if (exceptions.indexOf(f) === -1) {
          t.fail("non-string-literal config used in " + f + ":" + i)
        }
      })
    })
  })
  t.pass("got lines")
  t.end()
})

test("get docs", function (t) {
  var d = fs.readFileSync(doc, "utf8").split(/\r|\n/)
  // walk down until the "## Config Settings" section
  for (var i = 0; i < d.length && d[i] !== "## Config Settings"; i++);
  i++
  // now gather up all the ^###\s lines until the next ^##\s
  var doclines = []
  for (; i < d.length && !d[i].match(/^## /); i++) {
    if (d[i].match(/^### /))
      DOC[ d[i].replace(/^### /, '').trim() ] = true
  }
  t.pass("read the docs")
  t.end()
})

test("check configs", function (t) {
  var defs = require("npmconf/config-defs.js")
  var types = Object.keys(defs.types)
  var defaults = Object.keys(defs.defaults)

  for (var c in CONFS) {
    if (CONFS[c].file.indexOf(lib) === 0) {
      t.ok(DOC[c], "should be documented " + c + " "
          + CONFS[c].file + ":" + CONFS[c].line)
      t.ok(types.indexOf(c) !== -1, "should be defined in npmconf " + c)
      t.ok(defaults.indexOf(c) !== -1, "should have default in npmconf " + c)
    }
  }

  for (var c in DOC) {
    if (c !== "versions" && c !== "version" && c !== "init.version") {
      t.ok(CONFS[c], "config in doc should be used somewhere " + c)
      t.ok(types.indexOf(c) !== -1, "should be defined in npmconf " + c)
      t.ok(defaults.indexOf(c) !== -1, "should have default in npmconf " + c)
    }
  }

  types.forEach(function(c) {
    if (!c.match(/^\_/) && c !== 'argv' && !c.match(/^versions?$/)) {
      t.ok(DOC[c], 'defined type should be documented ' + c)
      t.ok(CONFS[c], 'defined type should be used ' + c)
    }
  })

  defaults.forEach(function(c) {
    if (!c.match(/^\_/) && c !== 'argv' && !c.match(/^versions?$/)) {
      t.ok(DOC[c], 'defaulted type should be documented ' + c)
      t.ok(CONFS[c], 'defaulted type should be used ' + c)
    }
  })

  t.end()
})
