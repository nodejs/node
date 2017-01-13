"use strict"
var test = require("tap").test
var requireInject = require("require-inject")
var path = require("path")

var re = {
  tarball: /[/\\]a.tar.gz$/,
  packagedir: /[/\\]b$/,
  packagejson: /[/\\]b[/\\]package.json$/,
  nonpackagedir: /[/\\]c$/,
  nopackagejson: /[/\\]c[/\\]package.json$/,
  remotename: /[/\\]d$/,
  packagedirlikegithub: /[/\\]e[/\\]1$/,
  packagejsonlikegithub: /[/\\]e[/\\]1[/\\]package.json$/,
  github: /[/\\]e[/\\]2$/,
  localrangefile: /[/\\]1[.]0[.]0$/,
  localverfile: /[/\\]1$/
}

var rps = requireInject("../index", {
  "fs": {
    "stat": function (path, callback) {
      if (re.tarball.test(path)) {
        callback(null,{isDirectory:function(){ return false }})
      }
      else if (re.packagedir.test(path)) {
        callback(null,{isDirectory:function(){ return true }})
      }
      else if (re.packagejson.test(path)) {
        callback(null,{})
      }
      else if (re.nonpackagedir.test(path)) {
        callback(null,{isDirectory:function(){ return true }})
      }
      else if (re.nopackagejson.test(path)) {
        callback(new Error("EFILENOTFOUND"))
      }
      else if (re.remotename.test(path)) {
        callback(new Error("EFILENOTFOUND"))
      }
      else if (re.packagedirlikegithub.test(path)) {
        callback(null,{isDirectory:function(){ return true }})
      }
      else if (re.packagejsonlikegithub.test(path)) {
        callback(null,{})
      }
      else if (re.github.test(path)) {
        callback(new Error("EFILENOTFOUND"))
      }
      else if (re.localverfile.test(path)) {
        callback(null,{isDirectory:function(){ return false }})
      }
      else if (re.localrangefile.test(path)) {
        callback(null,{isDirectory:function(){ return false }})
      }
      else {
        throw new Error("Unknown stat fixture path: "+path)
      }
    }
  }
})

test("realize-package-specifier", function (t) {
  t.plan(13)
  rps("a.tar.gz", function (err, result) {
    t.is(result.type, "local", "local tarball")
  })
  rps("b", function (err, result) {
    t.is(result.type, "directory", "local package directory")
  })
  rps("c", function (err, result) {
    t.is(result.type, "tag", "remote package, non-package local directory")
  })
  rps("d", function (err, result) {
    t.is(result.type, "tag", "remote package, no local directory")
  })
  rps("file:./a.tar.gz", function (err, result) {
    t.is(result.type, "local", "local tarball")
  })
  rps("file:./b", function (err, result) {
    t.is(result.type, "directory", "local package directory")
  })
  rps("file:./c", function (err, result) {
    t.is(result.type, "local", "non-package local directory, specified with a file URL")
  })
  rps("file:./d", function (err, result) {
    t.is(result.type, "local", "no local directory, specified with a file URL")
  })
  rps("e/1", function (err, result) {
    t.is(result.type, "directory", "local package directory")
  })
  rps("e/2", function (err, result) {
    t.is(result.type, "hosted", "hosted package dependency")
    t.is(result.hosted.type, "github", "github package dependency")
  })
  rps("1", function (err, result) {
    t.is(result.type, "local", "range like local file is still a local file")
  })
  rps("1.0.0", function (err, result) {
    t.is(result.type, "local", "version like local file is still a local file")
  })
})
test("named realize-package-specifier", function (t) {
  t.plan(13)

  rps("a@a.tar.gz", function (err, result) {
    t.is(result.type, "local", "named local tarball")
  })
  rps("b@b", function (err, result) {
    t.is(result.type, "directory", "named local package directory")
  })
  rps("c@c", function (err, result) {
    t.is(result.type, "tag", "remote package, non-package local directory")
  })
  rps("d@d", function (err, result) {
    t.is(result.type, "tag", "remote package, no local directory")
  })
  rps("a@file:./a.tar.gz", function (err, result) {
    t.is(result.type, "local", "local tarball")
  })
  rps("b@file:./b", function (err, result) {
    t.is(result.type, "directory", "local package directory")
  })
  rps("c@file:./c", function (err, result) {
    t.is(result.type, "local", "non-package local directory, specified with a file URL")
  })
  rps("d@file:./d", function (err, result) {
    t.is(result.type, "local", "no local directory, specified with a file URL")
  })
  rps("e@e/1", function (err, result) {
    t.is(result.type, "directory", "local package directory")
  })
  rps("e@e/2", function (err, result) {
    t.is(result.type, "hosted", "hosted package dependency")
    t.is(result.hosted.type, "github", "github package dependency")
  })
  rps("e@1", function (err, result) {
    t.is(result.type, "range", "range like specifier is never a local file")
  })
  rps("e@1.0.0", function (err, result) {
    t.is(result.type, "version", "version like specifier is never a local file")
  })
})
