var tap = require('tap')
var fs = require('fs')

var server = require('./lib/server.js')
var common = require('./lib/common.js')

var auth = {
  username: 'username',
  password: '%1234@asdf%',
  email: 'i@izs.me',
  alwaysAuth: true
}

var client = common.freshClient()

tap.test('publish again', function (t) {
  // not really a tarball, but doesn't matter
  var bodyPath = require.resolve('../package.json')
  var tarball = fs.createReadStream(bodyPath)
  var pd = fs.readFileSync(bodyPath)
  var pkg = require('../package.json')
  var lastTime = null

  server.expect('/npm-registry-client', function (req, res) {
    t.equal(req.method, 'PUT')
    var b = ''
    req.setEncoding('utf8')
    req.on('data', function (d) {
      b += d
    })

    req.on('end', function () {
      var o = lastTime = JSON.parse(b)
      t.equal(o._id, 'npm-registry-client')
      t.equal(o['dist-tags'].latest, pkg.version)
      t.has(o.versions[pkg.version], pkg)
      t.same(o.maintainers, [ { name: 'username', email: 'i@izs.me' } ])
      var att = o._attachments[ pkg.name + '-' + pkg.version + '.tgz' ]
      t.same(att.data, pd.toString('base64'))
      res.statusCode = 409
      res.json({reason: 'must supply latest _rev to update existing package'})
    })
  })

  server.expect('/npm-registry-client?write=true', function (req, res) {
    t.equal(req.method, 'GET')
    t.ok(lastTime)
    for (var i in lastTime.versions) {
      var v = lastTime.versions[i]
      delete lastTime.versions[i]
      lastTime.versions['0.0.2'] = v
      lastTime['dist-tags'] = { latest: '0.0.2' }
    }
    lastTime._rev = 'asdf'
    res.json(lastTime)
  })

  server.expect('/npm-registry-client', function (req, res) {
    t.equal(req.method, 'PUT')
    t.ok(lastTime)

    var b = ''
    req.setEncoding('utf8')
    req.on('data', function (d) {
      b += d
    })

    req.on('end', function () {
      var o = JSON.parse(b)
      t.equal(o._rev, 'asdf')
      t.deepEqual(o.versions['0.0.2'], o.versions[pkg.version])
      res.statusCode = 201
      res.json({created: true})
    })
  })

  var params = {
    metadata: pkg,
    access: 'public',
    body: tarball,
    auth: auth
  }
  client.publish('http://localhost:1337/', params, function (er, data) {
    if (er) throw er
    t.deepEqual(data, { created: true })
    server.close()
    t.end()
  })
})
