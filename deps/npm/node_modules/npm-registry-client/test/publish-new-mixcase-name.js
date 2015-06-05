var test = require('tap').test
var crypto = require('crypto')
var fs = require('fs')

var server = require('./lib/server.js')
var common = require('./lib/common.js')
var client = common.freshClient()

var URI = 'http://localhost:1337/'
var USERNAME = 'username'
var PASSWORD = '%1234@asdf%'
var EMAIL = 'i@izs.me'
var METADATA = require('../package.json')
var ACCESS = 'public'
// not really a tarball, but doesn't matter
var BODY_PATH = require.resolve('../package.json')
var BODY = fs.createReadStream(BODY_PATH)
var AUTH = {
  username: USERNAME,
  password: PASSWORD,
  email: EMAIL
}
var PARAMS = {
  metadata: METADATA,
  access: ACCESS,
  body: BODY,
  auth: AUTH
}

test('publish-new-mixcase-name', function (t) {
  var pd = fs.readFileSync(BODY_PATH, 'base64')

  // change to mixed-case name
  METADATA.name = 'npm-Registry-Client'

  server.expect('/npm-Registry-Client', function (req, res) {
    t.equal(req.method, 'PUT')
    var b = ''
    req.setEncoding('utf8')
    req.on('data', function (d) {
      b += d
    })

    req.on('end', function () {
      var o = JSON.parse(b)
      t.equal(o._id, 'npm-Registry-Client')
      t.equal(o['dist-tags'].latest, METADATA.version)
      t.equal(o.access, ACCESS)
      t.has(o.versions[METADATA.version], METADATA)
      t.same(o.maintainers, [{ name: 'username', email: 'i@izs.me' }])
      t.same(o.maintainers, o.versions[METADATA.version].maintainers)

      var att = o._attachments[METADATA.name + '-' + METADATA.version + '.tgz']
      t.same(att.data, pd)

      var hash = crypto.createHash('sha1').update(pd, 'base64').digest('hex')
      t.equal(o.versions[METADATA.version].dist.shasum, hash)

      res.statusCode = 403
      res.json({error: 'Name must be lower-case'})
    })
  })

  client.publish(URI, PARAMS, function (er, data, json, res) {
    t.assert(er instanceof Error) // expect error

    // TODO: need a test that ensures useful error message
    // t.similar(data.error, /must be lower-case/)

    t.end()
  })
})
