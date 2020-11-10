var fs = require('fs')
var ini = require('ini')
var t = require('tap')
const test = t.test
var common = require('../common-config.js')
var commonTap = require('../common-tap.js')
var npmconf = require('../../lib/config/core.js')

var expectConf = [
  'globalconfig = ' + common.globalconfig,
  'email = i@izs.me',
  'env-thing = foo',
  'init.author.name = Isaac Z. Schlueter',
  'init.author.email = i@izs.me',
  'init.author.url = http://blog.izs.me/',
  'init.version = 1.2.3',
  'npm:publishtest = true',
  '_npmjs.org:couch = https://admin:password@localhost:5984/registry',
  'npm-www:nocache = 1',
  'sign-git-tag = false',
  'message = v%s',
  'strict-ssl = false',
  '_auth = dXNlcm5hbWU6cGFzc3dvcmQ=',
  '',
  '[_token]',
  'AuthSession = yabba-dabba-doodle',
  'version = 1',
  'expires = 1345001053415',
  'path = /',
  'httponly = true',
  ''
].join('\n')

var expectFile = [
  'globalconfig = ' + common.globalconfig,
  'email = i@izs.me',
  'env-thing = foo',
  'init.author.name = Isaac Z. Schlueter',
  'init.author.email = i@izs.me',
  'init.author.url = http://blog.izs.me/',
  'init.version = 1.2.3',
  'npm:publishtest = true',
  '_npmjs.org:couch = https://admin:password@localhost:5984/registry',
  'npm-www:nocache = 1',
  'sign-git-tag = false',
  'message = v%s',
  'strict-ssl = false',
  '_auth = dXNlcm5hbWU6cGFzc3dvcmQ=',
  '',
  '[_token]',
  'AuthSession = yabba-dabba-doodle',
  'version = 1',
  'expires = 1345001053415',
  'path = /',
  'httponly = true',
  ''
].join('\n')

const userconfig = commonTap.pkg + '/userconfig'
fs.writeFileSync(userconfig, fs.readFileSync(common.userconfig))
process.env.npm_config_userconfig = userconfig

test('saving configs', function (t) {
  npmconf.load(function (er, conf) {
    if (er) throw er

    conf.set('sign-git-tag', false, 'user')
    conf.del('nodedir')
    conf.del('tmp')
    var foundConf = ini.stringify(conf.sources.user.data)
    t.same(ini.parse(foundConf), ini.parse(expectConf))
    fs.unlinkSync(common.userconfig)
    conf.save('user', function (er) {
      if (er) throw er

      var uc = fs.readFileSync(conf.get('userconfig'), 'utf8')
      t.same(ini.parse(uc), ini.parse(expectFile))
      t.end()
    })
  })
})

test('setting prefix', function (t) {
  npmconf.load(function (er, conf) {
    if (er) throw er

    conf.prefix = 'newvalue'
    t.same(conf.prefix, 'newvalue')
    t.end()
  })
})
