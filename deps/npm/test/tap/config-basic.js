var test = require('tap').test
var npmconf = require('../../lib/config/core.js')
var common = require('./00-config-setup.js')
var path = require('path')

var projectData = {
  'save-prefix': '~',
  'proprietary-attribs': false,
  'legacy-bundling': true
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
  if (aa == null && bb == null) return t.pass(msg)
  if (typeof bb !== 'object') return t.is(aa, bb, msg)
  if (!seen) seen = []
  for (var kk in seen) if (seen[kk] === aa || seen[kk] === bb) return
  seen.push(aa, bb)
  t.is(Object.keys(aa).length, Object.keys(bb).length, msg + ': # of elements')
  Object.keys(bb).forEach(function (key) {
    isDeeplyDetails(t, aa[key], bb[key], msg + ' -> ' + key, seen)
  })
}

test('no builtin', function (t) {
  t.comment(process.env)
  npmconf.load(cli, function (er, conf) {
    if (er) throw er
    expectNames.forEach(function (name, ii) {
      isDeeplyDetails(t, conf.list[ii], expectList[ii], 'config properities list: ' + name)
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
