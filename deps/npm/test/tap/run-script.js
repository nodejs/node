var common = require("../common-tap")
  , test = require("tap").test
  , path = require("path")
  , rimraf = require("rimraf")
  , mkdirp = require("mkdirp")
  , pkg = path.resolve(__dirname, "run-script")
  , cache = path.resolve(pkg, "cache")
  , tmp = path.resolve(pkg, "tmp")
  , opts = { cwd: pkg }

function testOutput (t, command, er, code, stdout, stderr) {
  var lines

  if (er)
    throw er

  if (stderr)
    throw new Error("npm " + command + " stderr: " + stderr.toString())

  lines = stdout.trim().split("\n")
  stdout = lines.filter(function(line) {
    return line.trim() !== "" && line[0] !== ">"
  }).join(";")

  t.equal(stdout, command)
  t.end()
}

function cleanup () {
  rimraf.sync(cache)
  rimraf.sync(tmp)
}

test("setup", function (t) {
  cleanup()
  mkdirp.sync(cache)
  mkdirp.sync(tmp)
  t.end()
})

test("npm run-script", function (t) {
  common.npm(["run-script", "start"], opts, testOutput.bind(null, t, "start"))
})

test("npm run-script with args", function (t) {
  common.npm(["run-script", "start", "--", "stop"], opts, testOutput.bind(null, t, "stop"))
})

test("npm run-script with args that contain spaces", function (t) {
  common.npm(["run-script", "start", "--", "hello world"], opts, testOutput.bind(null, t, "hello world"))
})

test("npm run-script with args that contain single quotes", function (t) {
  common.npm(["run-script", "start", "--", "they're awesome"], opts, testOutput.bind(null, t, "they're awesome"))
})

test("npm run-script with args that contain double quotes", function (t) {
  common.npm(["run-script", "start", "--", "what's \"up\"?"], opts, testOutput.bind(null, t, "what's \"up\"?"))
})

test("npm run-script with pre script", function (t) {
  common.npm(["run-script", "with-post"], opts, testOutput.bind(null, t, "main;post"))
})

test("npm run-script with post script", function (t) {
  common.npm(["run-script", "with-pre"], opts, testOutput.bind(null, t, "pre;main"))
})

test("npm run-script with both pre and post script", function (t) {
  common.npm(["run-script", "with-both"], opts, testOutput.bind(null, t, "pre;main;post"))
})

test("npm run-script with both pre and post script and with args", function (t) {
  common.npm(["run-script", "with-both", "--", "an arg"], opts, testOutput.bind(null, t, "pre;an arg;post"))
})

test("npm run-script explicitly call pre script with arg", function (t) {
  common.npm(["run-script", "prewith-pre", "--", "an arg"], opts, testOutput.bind(null, t, "an arg"))
})

test("npm run-script test", function (t) {
  common.npm(["run-script", "test"], opts, function (er, code, stdout, stderr) {
    t.ifError(er, "npm run-script test ran without issue")
    t.notOk(stderr, "should not generate errors")
    t.end()
  })
})

test("npm run-script env", function (t) {
  common.npm(["run-script", "env"], opts, function (er, code, stdout, stderr) {
    t.ifError(er, "using default env script")
    t.notOk(stderr, "should not generate errors")
    t.ok( stdout.indexOf("npm_config_init_version") > 0, "expected values in var list" )
    t.end()
  })
})

test("npm run-script nonexistent-script", function (t) {
  common.npm(["run-script", "nonexistent-script"], opts, function (er, code, stdout, stderr) {
    t.ifError(er, "npm run-script nonexistent-script did not cause npm to explode")
    t.ok(stderr, "should generate errors")
    t.end()
  })
})

test("cleanup", function (t) {
  cleanup()
  t.end()
})
