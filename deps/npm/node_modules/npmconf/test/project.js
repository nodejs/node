var test = require('tap').test
var npmconf = require('../npmconf.js')
var common = require('./00-setup.js')
var path = require('path')
var fix = path.resolve(__dirname, 'fixtures')
var projectRc = path.resolve(fix, '.npmrc')

var projectData = { just: 'testing' }

var ucData =
  { globalconfig: common.globalconfig,
    email: 'i@izs.me',
    'env-thing': 'asdf',
    'init.author.name': 'Isaac Z. Schlueter',
    'init.author.email': 'i@izs.me',
    'init.author.url': 'http://blog.izs.me/',
    'proprietary-attribs': false,
    'npm:publishtest': true,
    '_npmjs.org:couch': 'https://admin:password@localhost:5984/registry',
    _auth: 'dXNlcm5hbWU6cGFzc3dvcmQ=',
    'npm-www:nocache': '1',
    nodedir: '/Users/isaacs/dev/js/node-v0.8',
    'sign-git-tag': true,
    message: 'v%s',
    'strict-ssl': false,
    'tmp': process.env.HOME + '/.tmp',
    username : "username",
    _password : "password",
    _token:
     { AuthSession: 'yabba-dabba-doodle',
       version: '1',
       expires: '1345001053415',
       path: '/',
       httponly: true } }

var envData = common.envData
var envDataFix = common.envDataFix

var gcData = { 'package-config:foo': 'boo' }

var biData = {}

var cli = { foo: 'bar', umask: 022, prefix: fix }

var expectList =
[ cli,
  envDataFix,
  projectData,
  ucData,
  gcData,
  biData ]

var expectSources =
{ cli: { data: cli },
  env:
   { data: envDataFix,
     source: envData,
     prefix: '' },
  project:
    { path: projectRc,
      type: 'ini',
      data: projectData },
  user:
   { path: common.userconfig,
     type: 'ini',
     data: ucData },
  global:
   { path: common.globalconfig,
     type: 'ini',
     data: gcData },
  builtin: { data: biData } }

test('no builtin', function (t) {
  npmconf.load(cli, function (er, conf) {
    if (er) throw er
    t.same(conf.list, expectList)
    t.same(conf.sources, expectSources)
    t.same(npmconf.rootConf.list, [])
    t.equal(npmconf.rootConf.root, npmconf.defs.defaults)
    t.equal(conf.root, npmconf.defs.defaults)
    t.equal(conf.get('umask'), 022)
    t.equal(conf.get('heading'), 'npm')
    t.end()
  })
})
