var fs = require("fs")
var path = require("path")
var rimraf = require("rimraf")
var mr = require("npm-registry-mock")

var test = require("tap").test
var common = require("../common-tap.js")

var opts = {cwd : __dirname}
var outfile = path.resolve(__dirname, "_npmrc")
var responses = {
  "Username" : "u\n",
  "Password" : "p\n",
  "Email"    : "u@p.me\n"
}

function mocks(server) {
  server.filteringRequestBody(function (r) {
    if (r.match(/\"_id\":\"org\.couchdb\.user:u\"/)) {
      return "auth"
    }
  })
  server.put("/-/user/org.couchdb.user:u", "auth")
    .reply(201, {username : "u", password : "p", email : "u@p.me"})
}

test("npm login", function (t) {
  mr({port : common.port, mocks : mocks}, function (s) {
    var runner = common.npm(
    [
      "login",
      "--registry", common.registry,
      "--loglevel", "silent",
      "--userconfig", outfile
    ],
    opts,
    function (err, code) {
      t.notOk(code, "exited OK")
      t.notOk(err, "no error output")
      var config = fs.readFileSync(outfile, "utf8")
      t.like(config, /:always-auth=false/, "always-auth is scoped and false (by default)")
      s.close()
      rimraf(outfile, function (err) {
        t.ifError(err, "removed config file OK")
        t.end()
      })
    })

    var o = "", e = "", remaining = Object.keys(responses).length
    runner.stdout.on("data", function (chunk) {
      remaining--
      o += chunk

      var label = chunk.toString("utf8").split(":")[0]
      runner.stdin.write(responses[label])

      if (remaining === 0) runner.stdin.end()
    })
    runner.stderr.on("data", function (chunk) { e += chunk })
  })
})

test("npm login --always-auth", function (t) {
  mr({port : common.port, mocks : mocks}, function (s) {
    var runner = common.npm(
    [
      "login",
      "--registry", common.registry,
      "--loglevel", "silent",
      "--userconfig", outfile,
      "--always-auth"
    ],
    opts,
    function (err, code) {
      t.notOk(code, "exited OK")
      t.notOk(err, "no error output")
      var config = fs.readFileSync(outfile, "utf8")
      t.like(config, /:always-auth=true/, "always-auth is scoped and true")
      s.close()
      rimraf(outfile, function (err) {
        t.ifError(err, "removed config file OK")
        t.end()
      })
    })

    var o = "", e = "", remaining = Object.keys(responses).length
    runner.stdout.on("data", function (chunk) {
      remaining--
      o += chunk

      var label = chunk.toString("utf8").split(":")[0]
      runner.stdin.write(responses[label])

      if (remaining === 0) runner.stdin.end()
    })
    runner.stderr.on("data", function (chunk) { e += chunk })
  })
})

test("npm login --no-always-auth", function (t) {
  mr({port : common.port, mocks : mocks}, function (s) {
    var runner = common.npm(
    [
      "login",
      "--registry", common.registry,
      "--loglevel", "silent",
      "--userconfig", outfile,
      "--no-always-auth"
    ],
    opts,
    function (err, code) {
      t.notOk(code, "exited OK")
      t.notOk(err, "no error output")
      var config = fs.readFileSync(outfile, "utf8")
      t.like(config, /:always-auth=false/, "always-auth is scoped and false")
      s.close()
      rimraf(outfile, function (err) {
        t.ifError(err, "removed config file OK")
        t.end()
      })
    })

    var o = "", e = "", remaining = Object.keys(responses).length
    runner.stdout.on("data", function (chunk) {
      remaining--
      o += chunk

      var label = chunk.toString("utf8").split(":")[0]
      runner.stdin.write(responses[label])

      if (remaining === 0) runner.stdin.end()
    })
    runner.stderr.on("data", function (chunk) { e += chunk })
  })
})


test("cleanup", function (t) {
  rimraf.sync(outfile)
  t.pass("cleaned up")
  t.end()
})
