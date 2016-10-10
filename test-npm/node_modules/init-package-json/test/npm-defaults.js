var test = require('tap').test
var rimraf = require('rimraf')
var resolve = require('path').resolve

var npm = require('npm')
var init = require('../')

var EXPECTED = {
  name: 'test',
  version: '3.1.4',
  description: '',
  main: 'basic.js',
  scripts: {
    test: 'echo "Error: no test specified" && exit 1'
  },
  keywords: [],
  author: 'npmbot <n@p.m> (http://npm.im/)',
  license: 'WTFPL'
}

test('npm configuration values pulled from environment', function (t) {
  /*eslint camelcase:0 */
  process.env.npm_config_yes = 'yes'

  process.env.npm_config_init_author_name = 'npmbot'
  process.env.npm_config_init_author_email = 'n@p.m'
  process.env.npm_config_init_author_url = 'http://npm.im'

  process.env.npm_config_init_license = EXPECTED.license
  process.env.npm_config_init_version = EXPECTED.version

  npm.load({}, function (err) {
    t.ifError(err, 'npm loaded successfully')

    // clear out dotted names from test environment
    npm.config.del('init.author.name')
    npm.config.del('init.author.email')
    npm.config.del('init.author.url')
    // the following have npm defaults, and need to be explicitly overridden
    npm.config.set('init.license', '')
    npm.config.set('init.version', '')

    process.chdir(resolve(__dirname))
    init(__dirname, __dirname, npm.config, function (er, data) {
      t.ifError(err, 'init ran successfully')

      t.same(data, EXPECTED, 'got the package data from the environment')
      t.end()
    })
  })
})

test('npm configuration values pulled from dotted config', function (t) {
  /*eslint camelcase:0 */
  var config = {
    yes: 'yes',

    'init.author.name': 'npmbot',
    'init.author.email': 'n@p.m',
    'init.author.url': 'http://npm.im',

    'init.license': EXPECTED.license,
    'init.version': EXPECTED.version
  }

  npm.load(config, function (err) {
    t.ifError(err, 'npm loaded successfully')

    process.chdir(resolve(__dirname))
    init(__dirname, __dirname, npm.config, function (er, data) {
      t.ifError(err, 'init ran successfully')

      t.same(data, EXPECTED, 'got the package data from the config')
      t.end()
    })
  })
})

test('npm configuration values pulled from dashed config', function (t) {
  /*eslint camelcase:0 */
  var config = {
    yes: 'yes',

    'init-author-name': 'npmbot',
    'init-author-email': 'n@p.m',
    'init-author-url': 'http://npm.im',

    'init-license': EXPECTED.license,
    'init-version': EXPECTED.version
  }

  npm.load(config, function (err) {
    t.ifError(err, 'npm loaded successfully')

    process.chdir(resolve(__dirname))
    init(__dirname, __dirname, npm.config, function (er, data) {
      t.ifError(err, 'init ran successfully')

      t.same(data, EXPECTED, 'got the package data from the config')
      t.end()
    })
  })
})

test('cleanup', function (t) {
  rimraf.sync(resolve(__dirname, 'package.json'))
  t.pass('cleaned up')
  t.end()
})
