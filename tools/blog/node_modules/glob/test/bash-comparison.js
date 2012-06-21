// basic test
// show that it does the same thing by default as the shell.
var tap = require("tap")
, child_process = require("child_process")

// put more patterns here.
, globs =
  [
  "test/a/*/+(c|g)/./d"
  ,"test/a/**/[cg]/../[cg]"
  ,"test/a/{b,c,d,e,f}/**/g"
  ,"test/a/b/**"
  ,"test/**/g"
  ,"test/a/abc{fed,def}/g/h"
  ,"test/a/abc{fed/g,def}/**/"
  ,"test/a/abc{fed/g,def}/**///**/"
  ,"test/**/a/**/"
  ,"test/+(a|b|c)/a{/,bc*}/**"
  ,"test/*/*/*/f"
  ,"test/**/f"
  ,"test/a/symlink/a/b/c/a/b/c/a/b/c//a/b/c////a/b/c/**/b/c/**"
  ,"{./*/*,/usr/local/*}"
  ,"{/*,*}" // evil owl face!  how you taunt me!
  ]
, glob = require("../")
, path = require("path")

// run from the root of the project
// this is usually where you're at anyway, but be sure.
process.chdir(path.resolve(__dirname, ".."))

function alphasort (a, b) {
  a = a.toLowerCase()
  b = b.toLowerCase()
  return a > b ? 1 : a < b ? -1 : 0
}

globs.forEach(function (pattern) {
  var echoOutput
  tap.test(pattern, function (t) {
    var bashPattern = pattern
    , cmd = "shopt -s globstar && " +
            "shopt -s extglob && " +
            "shopt -s nullglob && " +
            // "shopt >&2; " +
            "eval \'for i in " + bashPattern + "; do echo $i; done\'"
    , cp = child_process.spawn("bash", ["-c",cmd])
    , out = []
    , globResult
    cp.stdout.on("data", function (c) {
      out.push(c)
    })
    cp.stderr.on("data", function (c) {
      process.stderr.write(c)
    })
    cp.stdout.on("close", function () {
      echoOutput = flatten(out)
      if (!echoOutput) echoOutput = []
      else {
        echoOutput = echoOutput.split(/\r*\n/).map(function (m) {
          // Bash is a oddly inconsistent with slashes in the
          // the results.  This implementation is a bit more
          // normalized.  Account for this in the test results.
          return m.replace(/\/+/g, "/").replace(/\/$/, "")
        }).sort(alphasort).reduce(function (set, f) {
          if (f !== set[set.length - 1]) set.push(f)
          return set
        }, []).sort(alphasort)
      }
      next()
    })

    glob(pattern, function (er, matches) {
      // sort and unpark, just to match the shell results
      matches = matches.map(function (m) {
        return m.replace(/\/+/g, "/").replace(/\/$/, "")
      }).sort(alphasort).reduce(function (set, f) {
        if (f !== set[set.length - 1]) set.push(f)
        return set
      }, []).sort(alphasort)

      t.ifError(er, pattern + " should not error")
      globResult = matches
      next()
    })

    function next () {
      if (!echoOutput || !globResult) return

      t.deepEqual(globResult, echoOutput, "should match shell")
      t.end()
    }
  })

  tap.test(pattern + " sync", function (t) {
    var matches = glob.sync(pattern).map(function (m) {
        return m.replace(/\/+/g, "/").replace(/\/$/, "")
      }).sort(alphasort).reduce(function (set, f) {
        if (f !== set[set.length - 1]) set.push(f)
        return set
      }, []).sort(alphasort)

    t.deepEqual(matches, echoOutput, "should match shell")
    t.end()
  })
})

function flatten (chunks) {
  var s = 0
  chunks.forEach(function (c) { s += c.length })
  var out = new Buffer(s)
  s = 0
  chunks.forEach(function (c) {
    c.copy(out, s)
    s += c.length
  })

  return out.toString().trim()
}
