// show that no match events happen while paused.
var tap = require("tap")
, child_process = require("child_process")
// just some gnarly pattern with lots of matches
, pattern = "test/a/symlink/a/b/c/a/b/c/a/b/c//a/b/c////a/b/c/**/b/c/**"
, glob = require("../")
, Glob = glob.Glob
, path = require("path")

// run from the root of the project
// this is usually where you're at anyway, but be sure.
process.chdir(path.resolve(__dirname, ".."))

function alphasort (a, b) {
  a = a.toLowerCase()
  b = b.toLowerCase()
  return a > b ? 1 : a < b ? -1 : 0
}

function cleanResults (m) {
  // normalize discrepancies in ordering, duplication,
  // and ending slashes.
  return m.map(function (m) {
    return m.replace(/\/+/g, "/").replace(/\/$/, "")
  }).sort(alphasort).reduce(function (set, f) {
    if (f !== set[set.length - 1]) set.push(f)
    return set
  }, []).sort(alphasort)
}

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
var bashResults
tap.test("get bash output", function (t) {
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
    bashResults = flatten(out)
    if (!bashResults) return t.fail("Didn't get results from bash")
    else {
      bashResults = cleanResults(bashResults.split(/\r*\n/))
    }
    t.ok(bashResults.length, "got some results")
    t.end()
  })
})

var globResults = []
tap.test("use a Glob object, and pause/resume it", function (t) {
  var g = new Glob(pattern)
  , paused = false
  , res = []

  g.on("match", function (m) {
    t.notOk(g.paused, "must not be paused")
    globResults.push(m)
    g.pause()
    t.ok(g.paused, "must be paused")
    setTimeout(g.resume.bind(g), 1)
  })

  g.on("end", function (matches) {
    t.pass("reached glob end")
    globResults = cleanResults(globResults)
    matches = cleanResults(matches)
    t.deepEqual(matches, globResults,
      "end event matches should be the same as match events")

    t.deepEqual(matches, bashResults,
      "glob matches should be the same as bash results")

    t.end()
  })
})

