var common = require('../common-tap.js')
var test = require('tap').test
var npm = require('../../')
var mkdirp = require('mkdirp')
var rimraf = require('rimraf')
var path = require('path')
var mr = require('npm-registry-mock')

var updateIndex = require('../../lib/cache/update-index.js')

var PKG_DIR = path.resolve(__dirname, 'get-basic')
var CACHE_DIR = path.resolve(PKG_DIR, 'cache')

var server

function setup (t, mock, extra) {
  mkdirp.sync(CACHE_DIR)
  mr({ port: common.port, plugin: mock }, function (er, s) {
    npm.load({ cache: CACHE_DIR, registry: common.registry }, function (err) {
      if (extra) {
        Object.keys(extra).forEach(function (k) {
          npm.config.set(k, extra[k], 'user')
        })
      }
      t.ifError(err, 'no error')
      server = s
      t.end()
    })
  })
}

function cleanup (t) {
  server.close(function () {
    rimraf.sync(PKG_DIR)

    t.end()
  })
}

test('setup basic', function (t) {
  setup(t, mocks.basic)
})

test('request basic', function (t) {
  updateIndex(0, function (er) {
    t.ifError(er, 'no error')
    t.end()
  })
})

test('cleanup basic', cleanup)

test('setup auth', function (t) {
  setup(t, mocks.auth)
})

test('request auth failure', function (t) {
  updateIndex(0, function (er) {
    t.equals(er.code, 'E401', 'gotta get that auth')
    t.ok(/^unauthorized/.test(er.message), 'unauthorized message')
    t.end()
  })
})

test('cleanup auth failure', cleanup)

test('setup auth', function (t) {
  // mimic as if alwaysAuth had been set
  setup(t, mocks.auth, {
    _auth: new Buffer('bobby:tables').toString('base64'),
    'always-auth': true
  })
})

test('request auth success', function (t) {
  updateIndex(0, function (er) {
    t.ifError(er, 'no error')
    t.end()
  })
})

test('cleanup auth', cleanup)

var mocks = {
  basic: function (mock) {
    mock.get('/-/all').reply(200, allMock)
  },
  auth: function (mock) {
    var littleBobbyTablesAuth = new Buffer('bobby:tables').toString('base64')
    var auth = 'Basic ' + littleBobbyTablesAuth
    mock.get('/-/all', { authorization: auth }).reply(200, allMock)
    mock.get('/-/all').reply(401, {
      error: 'unauthorized',
      reason: 'You are not authorized to access this db.'
    })
  }
}

var allMock = {
  '_updated': 1411727900 + 25,
  'generator-frontcow': {
      'name': 'generator-frontcow',
      'description': 'f36b6a6123da50959741e2ce4d634f96ec668c56 This is a fake description to ensure we do not accidentally search the real npm registry or use some kind of cache',
      'dist-tags': {
          'latest': '0.1.19'
      },
      'maintainers': [
          {
              'name': 'bcabanes',
              'email': 'contact@benjamincabanes.com'
          }
      ],
      'homepage': 'https://github.com/bcabanes/generator-frontcow',
      'keywords': [
          'sass',
          'frontend',
          'yeoman-generator',
          'atomic',
          'design',
          'sass',
          'foundation',
          'foundation5',
          'atomic design',
          'bourbon',
          'polyfill',
          'font awesome'
      ],
      'repository': {
          'type': 'git',
          'url': 'https://github.com/bcabanes/generator-frontcow'
      },
      'author': {
          'name': 'ben',
          'email': 'contact@benjamincabanes.com',
          'url': 'https://github.com/bcabanes'
      },
      'bugs': {
          'url': 'https://github.com/bcabanes/generator-frontcow/issues'
      },
      'license': 'MIT',
      'readmeFilename': 'README.md',
      'time': {
          'modified': '2014-10-03T02:26:18.406Z'
      },
      'versions': {
          '0.1.19': 'latest'
      }
  },
  'marko': {
      'name': 'marko',
      'description': 'Marko is an extensible, streaming, asynchronous, high performance, HTML-based templating language that can be used in Node.js or in the browser.',
      'dist-tags': {
          'latest': '1.2.16'
      },
      'maintainers': [
          {
              'name': 'pnidem',
              'email': 'pnidem@gmail.com'
          },
          {
              'name': 'philidem',
              'email': 'phillip.idem@gmail.com'
          }
      ],
      'homepage': 'https://github.com/raptorjs/marko',
      'keywords': [
          'templating',
          'template',
          'async',
          'streaming'
      ],
      'repository': {
          'type': 'git',
          'url': 'https://github.com/raptorjs/marko.git'
      },
      'author': {
          'name': 'Patrick Steele-Idem',
          'email': 'pnidem@gmail.com'
      },
      'bugs': {
          'url': 'https://github.com/raptorjs/marko/issues'
      },
      'license': 'Apache License v2.0',
      'readmeFilename': 'README.md',
      'users': {
          'pnidem': true
      },
      'time': {
          'modified': '2014-10-03T02:27:31.775Z'
      },
      'versions': {
          '1.2.16': 'latest'
      }
  }
}
