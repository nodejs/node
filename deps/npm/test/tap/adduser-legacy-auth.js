var fs = require('fs')
var path = require('path')
var mkdirp = require('mkdirp')
var rimraf = require('rimraf')
var mr = require('npm-registry-mock')

var test = require('tap').test
var common = require('../common-tap.js')

var opts = { cwd: common.pkg }
var pkg = common.pkg
var outfile = path.resolve(pkg, '_npmrc')

var contents = '_auth=' + Buffer.from('u:x').toString('base64') + '\n' +
               'registry=https://nonexistent.lvh.me/registry\n' +
               'email=u@p.me\n'

var responses = {
  'Username': 'u\n',
  'Password': 'p\n',
  'Email': 'u@p.me\n'
}

function mocks (server) {
  server.filteringRequestBody(function (r) {
    if (r.match(/"_id":"org\.couchdb\.user:u"/)) {
      return 'auth'
    } else {
      return 'invalid'
    }
  })
  server.post('/-/v1/login', 'invalid').reply(404, 'not found')
  server.put('/-/user/org.couchdb.user:u', 'auth')
    .reply(409, { error: 'user exists' })
  server.get('/-/user/org.couchdb.user:u?write=true')
    .reply(200, { _rev: '3-deadcafebabebeef' })
  server.put(
    '/-/user/org.couchdb.user:u/-rev/3-deadcafebabebeef',
    'auth',
    { authorization: 'Basic dTpw' }
  ).reply(201, { username: 'u', password: 'p', email: 'u@p.me' })
}

test('setup', function (t) {
  rimraf.sync(pkg)
  mkdirp(pkg, function (er) {
    t.ifError(er, pkg + ' made successfully')

    fs.writeFile(outfile, contents, function (er) {
      t.ifError(er, 'wrote legacy config')

      t.end()
    })
  })
})

test('npm login', function (t) {
  mr({ port: common.port, plugin: mocks }, function (er, s) {
    var runner = common.npm(
      [
        'login',
        '--registry', common.registry,
        '--loglevel', 'error',
        '--userconfig', outfile
      ],
      opts,
      function (err, code, stdout, stderr) {
        if (err) throw err
        t.is(stderr, '', 'no error output')
        t.is(code, 0, 'exited OK')
        var config = fs.readFileSync(outfile, 'utf8')
        t.like(config, /:always-auth=false/, 'always-auth is scoped and false (by default)')
        s.close()
        rimraf(outfile, function (err) {
          t.ifError(err, 'removed config file OK')
          t.end()
        })
      }
    )

    var remaining = Object.keys(responses).length
    runner.stdout.on('data', function (chunk) {
      if (remaining > 0) {
        remaining--

        var label = chunk.toString('utf8').split(':')[0]
        if (responses[label]) runner.stdin.write(responses[label])

        if (remaining === 0) runner.stdin.end()
      } else {
        var message = chunk.toString('utf8').trim()
        t.equal(message, 'Logged in as u on ' + common.registry + '/.')
      }
    })
  })
})

test('cleanup', function (t) {
  rimraf.sync(pkg)
  t.pass('cleaned up')
  t.end()
})
