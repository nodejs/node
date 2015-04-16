var path = require('path')

var tap = require('tap')

var readJson = require('../')

var expect = {
  name: 'read-package-json',
  version: '0.1.1',
  author: {
    name: 'Isaac Z. Schlueter',
    email: 'i@izs.me',
    url: 'http://blog.izs.me/'
  },
  description: 'The thing npm uses to read package.json files with semantics and defaults and validation',
  repository: {
    type: 'git',
    url: 'git://github.com/isaacs/read-package-json.git'
  },
  bugs: {
    url: 'https://github.com/isaacs/read-package-json/issues'
  },
  main: 'read-json.js',
  scripts: { test: 'tap test/*.js' },
  dependencies: {
    glob: '~3.1.9',
    'lru-cache': '~1.1.0',
    semver: '~1.0.14',
    slide: '~1.1.3',
    npmlog: '0',
    'graceful-fs': '~1.1.8'
  },
  devDependencies: { tap: '~0.2.5' },
  homepage: 'https://github.com/isaacs/read-package-json',
  optionalDependencies: { npmlog: '0', 'graceful-fs': '~1.1.8' },
  _id: 'read-package-json@0.1.1',
  readme: 'ERROR: No README data found!'
}

tap.test('from css', function (t) {
  var c = path.join(__dirname, 'fixtures', 'not-json.css')
  readJson(c, function (er, d) {
    t.same(d, expect)
    t.end()
  })
})

tap.test('from js', function (t) {
  readJson(__filename, function (er, d) {
    t.same(d, expect)
    t.end()
  })
})

/**package
{
  "name": "read-package-json",
  "version": "0.1.1",
  "author": "Isaac Z. Schlueter <i@izs.me> (http://blog.izs.me/)",
  "description": "The thing npm uses to read package.json files with semantics and defaults and validation",
  "repository": {
    "type": "git",
    "url": "git://github.com/isaacs/read-package-json.git"
  },
  "main": "read-json.js",
  "scripts": {
    "test": "tap test/*.js"
  },
  "dependencies": {
    "glob": "~3.1.9",
    "lru-cache": "~1.1.0",
    "semver": "~1.0.14",
    "slide": "~1.1.3"
  },
  "devDependencies": {
    "tap": "~0.2.5"
  },
  "optionalDependencies": {
    "npmlog": "0",
    "graceful-fs": "~1.1.8"
  }
}
**/
