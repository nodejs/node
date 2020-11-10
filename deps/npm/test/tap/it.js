var join = require('path').join
var statSync = require('graceful-fs').statSync
var writeFileSync = require('graceful-fs').writeFileSync

var mr = require('npm-registry-mock')
var rimraf = require('rimraf')
var test = require('tap').test

var common = require('../common-tap')

var pkg = common.pkg
var installed = join(pkg, 'node_modules', 'underscore', 'package.json')

var json = {
  name: 'npm-it-test',
  dependencies: {
    underscore: '1.5.1'
  },
  scripts: {
    test: 'echo hax'
  }
}

test('run up the mock registry', function (t) {
  mr({ port: common.port }, function (err, s) {
    if (err) throw err
    t.parent.teardown(() => s.close())
    t.end()
  })
})

const check = args => t =>
  common.npm(args.concat('--registry=' + common.registry), { cwd: pkg })
    .then(([code, stdout, stderr]) => {
      t.equal(code, 0, 'command ran without error')
      t.ok(statSync(installed), 'package was installed')
      t.equal(require(installed).version, '1.5.1', 'underscore got installed as expected')
      t.match(stdout, /hax/, 'found expected test output')
      t.notOk(stderr, 'stderr should be empty')
    })

test('npm install-test', function (t) {
  t.plan(2)
  t.test('setup', setup)
  t.test('check', check(['install-test', '--no-shrinkwrap']))
})

test('npm it (the form most people will use)', function (t) {
  t.plan(2)
  t.test('setup', setup)
  t.test('check', check(['it']))
})

function setup (t) {
  t.test('destroy', t => {
    t.plan(2)
    rimraf(join(pkg, 'node_modules'), () => t.pass('node_modules'))
    rimraf(join(pkg, 'package-lock.json'), () => t.pass('lock file'))
  })
  t.test('create', t => {
    writeFileSync(join(pkg, 'package.json'), JSON.stringify(json, null, 2))
    t.end()
  })
  t.end()
}
