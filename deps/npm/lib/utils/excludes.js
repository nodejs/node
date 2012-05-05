// build up a set of exclude lists in order of precedence:
// [ ["!foo", "bar"]
// , ["foo", "!bar"] ]
// being *included* will override a previous exclusion,
// and being excluded will override a previous inclusion.
//
// Each time the tar file-list generator thingie enters a new directory,
// it calls "addIgnoreFile(dir, list, cb)".  If an ignore file is found,
// then it is added to the list and the cb() is called with an
// child of the original list, so that we don't have
// to worry about popping it off at the right time, since other
// directories will continue to use the original parent list.
//
// If no ignore file is found, then the original list is returned.
//
// To start off with, ~/.{npm,git}ignore is added, as is
// prefix/{npm,git}ignore, effectively treated as if they were in the
// base package directory.

exports.addIgnoreFile = addIgnoreFile
exports.readIgnoreFile = readIgnoreFile
exports.parseIgnoreFile = parseIgnoreFile
exports.test = test
exports.filter = filter

var path = require("path")
  , fs = require("graceful-fs")
  , minimatch = require("minimatch")
  , relativize = require("./relativize.js")
  , log = require("./log.js")

// todo: memoize

// read an ignore file, or fall back to the
// "gitBase" file in the same directory.
function readIgnoreFile (file, gitBase, cb) {
  //log.warn(file, "ignoreFile")
  if (!file) return cb(null, "")
  fs.readFile(file, function (er, data) {
    if (!er || !gitBase) return cb(null, data || "")
    var gitFile = path.resolve(path.dirname(file), gitBase)
    fs.readFile(gitFile, function (er, data) {
      return cb(null, data || "")
    })
  })
}

// read a file, and then return the list of patterns
function parseIgnoreFile (file, gitBase, dir, cb) {
  readIgnoreFile(file, gitBase, function (er, data) {
    data = data ? data.toString("utf8") : ""

    data = data.split(/[\r\n]+/).map(function (p) {
      return p.trim()
    }).filter(function (p) {
      return p.length && p.charAt(0) !== "#"
    })
    data.dir = dir
    return cb(er, data)
  })
}

// add an ignore file to an existing list which can
// then be passed to the test() function. If the ignore
// file doesn't exist, then the list is unmodified. If
// it is, then a concat-child of the original is returned,
// so that this is suitable for walking a directory tree.
function addIgnoreFile (file, gitBase, list, dir, cb) {
  if (typeof cb !== "function") cb = dir, dir = path.dirname(file)
  if (typeof cb !== "function") cb = list, list = []
  parseIgnoreFile(file, gitBase, dir, function (er, data) {
    if (!er && data) {
      // package.json "files" array trumps everything
      // Make sure it's always last.
      if (list.length && list[list.length-1].packageFiles) {
        list = list.concat([data, list.pop()])
      } else {
        list = list.concat([data])
      }
    }
    cb(er, list)
  })
}


// no IO
// loop through the lists created in the functions above, and test to
// see if a file should be included or not, given those exclude lists.
function test (file, excludeList) {
  if (path.basename(file) === "package.json") return true
  // log.warn(file, "test file")
  // log.warn(excludeList, "test list")
  var incRe = /^\!(\!\!)*/
    , excluded = false
  for (var i = 0, l = excludeList.length; i < l; i ++) {
    var excludes = excludeList[i]
      , dir = excludes.dir

    // chop the filename down to be relative to excludeDir
    var rf = relativize(file, dir, true)
    rf = rf.replace(/^\.?\//, "")
    if (file.slice(-1) === "/") rf += "/"

    // log.warn([file, rf], "rf")

    for (var ii = 0, ll = excludes.length; ii < ll; ii ++) {
      var ex = excludes[ii].replace(/^(!*)\//, "$1")
        , inc = !!ex.match(incRe)

      // log.warn([ex, rf], "ex, rf")
      // excluding/including a dir excludes/includes all the files in it.
      if (ex.slice(-1) === "/") ex += "**"

      // if this is not an inclusion attempt, and someone else
      // excluded it, then just continue, because there's nothing
      // that can be done here to change the exclusion.
      if (!inc && excluded) continue

      // if it's an inclusion attempt, and the file has not been
      // excluded, then skip it, because there's no need to try again.
      if (inc && !excluded) continue

      // if it matches the pattern, then it should be excluded.
      excluded = !!minimatch(rf, ex, { matchBase: true })
      // log.error([rf, ex, excluded], "rf, ex, excluded")

      // if you include foo, then it also includes foo/bar.js
      if (inc && excluded && ex.slice(-3) !== "/**") {
        excluded = minimatch(rf, ex + "/**", { matchBase: true })
        // log.warn([rf, ex + "/**", inc, excluded], "dir without /")
      }

      // if you exclude foo, then it also excludes foo/bar.js
      if (!inc
          && excluded
          && ex.slice(-3) !== "/**"
          && rf.slice(-1) === "/"
          && excludes.indexOf(ex + "/**") === -1) {
        // log.warn(ex + "/**", "adding dir-matching exclude pattern")
        excludes.splice(ii, 1, ex, ex + "/**")
        ll ++
      }
    }
    // log.warn([rf, excluded, excludes], "rf, excluded, excludes")
  }
  // true if it *should* be included
  // log.warn([file, excludeList, excluded], "file, excluded")
  return !excluded
}

// returns a function suitable for Array#filter
function filter (dir, list) { return function (file) {
  file = file.trim()
  var testFile = path.resolve(dir, file)
  if (file.slice(-1) === "/") testFile += "/"
  return file && test(testFile, list)
}}
