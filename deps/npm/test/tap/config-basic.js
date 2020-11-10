var test = require('tap').test
var common = require('../common-config.js')
var npmconf = require('../../lib/config/core.js')
var path = require('path')

var projectData = {
}

var ucData = common.ucData
var envData = common.envData

var gcData = { 'package-config:foo': 'boo' }

var biData = {}

var cli = { foo: 'bar', umask: parseInt('022', 8) }

var expectNames = [
  'cli',
  'envData',
  'projectData',
  'ucData',
  'gcData',
  'biData'
]
var expectList = [
  cli,
  envData,
  projectData,
  ucData,
  gcData,
  biData
]

var expectSources = {
  cli: { data: cli },
  env: {
    data: envData,
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

function isDeeplyDetails (t, aa, bb, msg, seen) {
  return t.same(aa, bb, msg)
}

test('no builtin', function (t) {
  t.comment(process.env)
  npmconf.load(cli, function (er, conf) {
    if (er) throw er
    expectNames.forEach(function (name, ii) {
      isDeeplyDetails(t, conf.list[ii], expectList[ii], 'config properties list: ' + name)
    })
    isDeeplyDetails(t, conf.sources, expectSources, 'config by source')
    t.same(npmconf.rootConf.list, [], 'root configuration is empty')
    isDeeplyDetails(t, npmconf.rootConf.root, npmconf.defs.defaults, 'defaults')
    isDeeplyDetails(t, conf.root, npmconf.defs.defaults, 'current root config is defaults')
    t.is(conf.get('umask'), parseInt('022', 8), 'umask is as expected')
    t.is(conf.get('heading'), 'npm', 'config name is as expected')
    t.end()
  })
})
