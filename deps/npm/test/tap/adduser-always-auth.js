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

function verifyStdout (runner, successMessage, t) {
  var remaining = Object.keys(responses).length
  return function (chunk) {
    if (remaining > 0) {
      remaining--

      var label = chunk.toString('utf8').split(':')[0]
      runner.stdin.write(responses[label])

      if (remaining === 0) runner.stdin.end()
    } else {
      var message = chunk.toString('utf8').trim()
      t.equal(message, successMessage)
    }
  }
}

function mocks (server) {
  server.filteringRequestBody(function (r) {
    if (r.match(/\"_id\":\"org\.couchdb\.user:u\"/)) {
      return "auth"
    }
  })
  server.put("/-/user/org.couchdb.user:u", "auth")
    .reply(201, {username : "u", password : "p", email : "u@p.me"})
}

test("npm login", function (t) {
  mr({port : common.port, plugin : mocks}, function (er, s) {
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

    var message = 'Logged in as u on ' + common.registry + '/.'
    runner.stdout.on('data', verifyStdout(runner, message, t))
  })
})

test('npm login --scope <scope> uses <scope>:registry as its URI', function (t) {
  var port = common.port + 1
  var uri = 'http://localhost:' + port + '/'
  var scope = '@myco'
  common.npm(
    [
      'config',
      '--userconfig', outfile,
      'set',
      scope + ':registry',
      uri
    ],
  opts,
  function (err, code) {
    t.notOk(code, 'exited OK')
    t.notOk(err, 'no error output')

    mr({ port: port, plugin: mocks }, function (er, s) {
      var runner = common.npm(
        [
          'login',
          '--loglevel', 'silent',
          '--userconfig', outfile,
          '--scope', scope
        ],
      opts,
      function (err, code) {
        t.notOk(code, 'exited OK')
        t.notOk(err, 'no error output')
        var config = fs.readFileSync(outfile, 'utf8')
        t.like(config, new RegExp(scope + ':registry=' + uri), 'scope:registry is set')
        s.close()
        rimraf(outfile, function (err) {
          t.ifError(err, 'removed config file OK')
          t.end()
        })
      })

      var message = 'Logged in as u to scope ' + scope + ' on ' + uri + '.'
      runner.stdout.on('data', verifyStdout(runner, message, t))
    })
  })
})

test('npm login --scope <scope> makes sure <scope> is prefixed by an @', function (t) {
  var port = common.port + 1
  var uri = 'http://localhost:' + port + '/'
  var scope = 'myco'
  var prefixedScope = '@' + scope
  common.npm(
    [
      '--userconfig', outfile,
      'config',
      'set',
      prefixedScope + ':registry',
      uri
    ],
  opts,
  function (err, code) {
    t.notOk(code, 'exited OK')
    t.notOk(err, 'no error output')

    mr({ port: port, plugin: mocks }, function (er, s) {
      var runner = common.npm(
        [
          'login',
          '--loglevel', 'silent',
          '--userconfig', outfile,
          '--scope', scope
        ],
      opts,
      function (err, code) {
        t.notOk(code, 'exited OK')
        t.notOk(err, 'no error output')
        var config = fs.readFileSync(outfile, 'utf8')
        t.like(config, new RegExp(prefixedScope + ':registry=' + uri), 'scope:registry is set')
        s.close()
        rimraf(outfile, function (err) {
          t.ifError(err, 'removed config file OK')
          t.end()
        })
      })

      var message = 'Logged in as u to scope ' + prefixedScope + ' on ' + uri + '.'
      runner.stdout.on('data', verifyStdout(runner, message, t))
    })
  })
})

test('npm login --scope <scope> --registry <registry> uses <registry> as its URI', function (t) {
  var scope = '@myco'
  common.npm(
    [
      '--userconfig', outfile,
      'config',
      'set',
      scope + ':registry',
      'invalidurl'
    ],
  opts,
  function (err, code) {
    t.notOk(code, 'exited OK')
    t.notOk(err, 'no error output')

    mr({ port: common.port, plugin: mocks }, function (er, s) {
      var runner = common.npm(
        [
          'login',
          '--registry', common.registry,
          '--loglevel', 'silent',
          '--userconfig', outfile,
          '--scope', scope
        ],
      opts,
      function (err, code) {
        t.notOk(code, 'exited OK')
        t.notOk(err, 'no error output')
        var config = fs.readFileSync(outfile, 'utf8')
        t.like(config, new RegExp(scope + ':registry=' + common.registry), 'scope:registry is set')
        s.close()
        rimraf(outfile, function (err) {
          t.ifError(err, 'removed config file OK')
          t.end()
        })
      })

      var message = 'Logged in as u to scope ' + scope + ' on ' + common.registry + '/.'
      runner.stdout.on('data', verifyStdout(runner, message, t))
    })
  })
})

test("npm login --always-auth", function (t) {
  mr({port : common.port, plugin : mocks}, function (er, s) {
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

    var message = 'Logged in as u on ' + common.registry + '/.'
    runner.stdout.on('data', verifyStdout(runner, message, t))
  })
})

test("npm login --no-always-auth", function (t) {
  mr({port : common.port, plugin : mocks}, function (er, s) {
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

    var message = 'Logged in as u on ' + common.registry + '/.'
    runner.stdout.on('data', verifyStdout(runner, message, t))
  })
})


test("cleanup", function (t) {
  rimraf.sync(outfile)
  t.pass("cleaned up")
  t.end()
})
