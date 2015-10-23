var test = require('tap').test
var npmconf = require('../../lib/config/core.js')
var common = require('./00-config-setup.js')
var path = require('path')

var projectData = {
  'save-prefix': '~',
  'proprietary-attribs': false
}

var ucData = common.ucData
var envData = common.envData
var envDataFix = common.envDataFix

var gcData = { 'package-config:foo': 'boo' }

var biData = {}

var cli = { foo: 'bar', umask: parseInt('022', 8) }

var expectList = [
  cli,
  envDataFix,
  projectData,
  ucData,
  gcData,
  biData
]

var expectSources = {
  cli: { data: cli },
  env: {
    data: envDataFix,
    source: envData,
    prefix: ''
  },
  project: {
    path: path.resolve(__dirname, '..', '..', '.npmrc'),
    type: 'ini',
    data: projectData
  },
  user: {
    path: common.userconfig,
    type: 'ini',
    data: ucData
  },
  global: {
    path: common.globalconfig,
    type: 'ini',
    data: gcData
  },
  builtin: { data: biData }
}

test('no builtin', function (t) {
  npmconf.load(cli, function (er, conf) {
    if (er) throw er
    t.same(conf.list, expectList)
    t.same(conf.sources, expectSources)
    t.same(npmconf.rootConf.list, [])
    t.equal(npmconf.rootConf.root, npmconf.defs.defaults)
    t.equal(conf.root, npmconf.defs.defaults)
    t.equal(conf.get('umask'), parseInt('022', 8))
    t.equal(conf.get('heading'), 'npm')
    t.end()
  })
})
