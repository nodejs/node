var fs = require('fs')
var glob = require('glob')
var path = require('path')
var validateLicense = require('validate-npm-package-license')
var validateName = require('validate-npm-package-name')
var npa = require('npm-package-arg')
var semver = require('semver')

// more popular packages should go here, maybe?
function isTestPkg (p) {
  return !!p.match(/^(expresso|mocha|tap|coffee-script|coco|streamline)$/)
}

function niceName (n) {
  return n.replace(/^node-|[.-]js$/g, '').replace(/\s+/g, ' ').replace(/ /g, '-').toLowerCase()
}

function readDeps (test, excluded) { return function (cb) {
  fs.readdir('node_modules', function (er, dir) {
    if (er) return cb()
    var deps = {}
    var n = dir.length
    if (n === 0) return cb(null, deps)
    dir.forEach(function (d) {
      if (d.match(/^\./)) return next()
      if (test !== isTestPkg(d) || excluded[d])
        return next()

      var dp = path.join(dirname, 'node_modules', d, 'package.json')
      fs.readFile(dp, 'utf8', function (er, p) {
        if (er) return next()
        try { p = JSON.parse(p) }
        catch (e) { return next() }
        if (!p.version) return next()
        if (p._requiredBy) {
          if (!p._requiredBy.some(function (req) { return req === '#USER' })) return next()
        }
        deps[d] = config.get('save-exact') ? p.version : config.get('save-prefix') + p.version
        return next()
      })
    })
    function next () {
      if (--n === 0) return cb(null, deps)
    }
  })
}}

var name = niceName(package.name || basename)
var spec
try {
  spec = npa(name)
} catch (e) {
  spec = {}
}
var scope = config.get('scope')
if (scope) {
  if (scope.charAt(0) !== '@') scope = '@' + scope
  if (spec.scope) {
    name = scope + '/' + spec.name.split('/')[1]
  } else {
    name = scope + '/' + name
  }
}
exports.name =  yes ? name : prompt('package name', name, function (data) {
  var its = validateName(data)
  if (its.validForNewPackages) return data
  var errors = (its.errors || []).concat(its.warnings || [])
  var er = new Error('Sorry, ' + errors.join(' and ') + '.')
  er.notValid = true
  return er
})

const defaultDottedInitVersion = config &&
  config.defaults &&
  config.defaults['init.version']
const dottedInitVersion =
  config.get('init.version') !== defaultDottedInitVersion &&
  config.get('init.version')
var version = package.version ||
              dottedInitVersion ||
              config.get('init-version') ||
              '1.0.0'
exports.version = yes ?
  version :
  prompt('version', version, function (version) {
    if (semver.valid(version)) return version
    var er = new Error('Invalid version: "' + version + '"')
    er.notValid = true
    return er
  })

if (!package.description) {
  exports.description = yes ? '' : prompt('description')
}

if (!package.main) {
  exports.main = function (cb) {
    fs.readdir(dirname, function (er, f) {
      if (er) f = []

      f = f.filter(function (f) {
        return f.match(/\.js$/)
      })

      if (f.indexOf('index.js') !== -1)
        f = 'index.js'
      else if (f.indexOf('main.js') !== -1)
        f = 'main.js'
      else if (f.indexOf(basename + '.js') !== -1)
        f = basename + '.js'
      else
        f = f[0]

      var index = f || 'index.js'
      return cb(null, yes ? index : prompt('entry point', index))
    })
  }
}

if (!package.bin) {
  exports.bin = function (cb) {
    fs.readdir(path.resolve(dirname, 'bin'), function (er, d) {
      // no bins
      if (er) return cb()
      // just take the first js file we find there, or nada
      return cb(null, d.filter(function (f) {
        return f.match(/\.js$/)
      })[0])
    })
  }
}

exports.directories = function (cb) {
  fs.readdir(dirname, function (er, dirs) {
    if (er) return cb(er)
    var res = {}
    dirs.forEach(function (d) {
      switch (d) {
        case 'example': case 'examples': return res.example = d
        case 'test': case 'tests': return res.test = d
        case 'doc': case 'docs': return res.doc = d
        case 'man': return res.man = d
        case 'lib': return res.lib = d
      }
    })
    if (Object.keys(res).length === 0) res = undefined
    return cb(null, res)
  })
}

if (!package.dependencies) {
  exports.dependencies = readDeps(false, package.devDependencies || {})
}

if (!package.devDependencies) {
  exports.devDependencies = readDeps(true, package.dependencies || {})
}

// MUST have a test script!
var s = package.scripts || {}
var notest = 'echo "Error: no test specified" && exit 1'
if (!package.scripts) {
  exports.scripts = function (cb) {
    fs.readdir(path.join(dirname, 'node_modules'), function (er, d) {
      setupScripts(d || [], cb)
    })
  }
}
function setupScripts (d, cb) {
  // check to see what framework is in use, if any
  function tx (test) {
    return test || notest
  }
  if (!s.test || s.test === notest) {
    var commands = {
      'tap':'tap test/*.js'
    , 'expresso':'expresso test'
    , 'mocha':'mocha'
    }
    var command
    Object.keys(commands).forEach(function (k) {
      if (d.indexOf(k) !== -1) command = commands[k]
    })
    var ps = 'test command'
    if (yes) {
      s.test = command || notest
    } else {
      s.test = command ? prompt(ps, command, tx) : prompt(ps, tx)
    }
  }
  return cb(null, s)
}

if (!package.repository) {
  exports.repository = function (cb) {
    fs.readFile('.git/config', 'utf8', function (er, gconf) {
      if (er || !gconf) {
        return cb(null, yes ? '' : prompt('git repository'))
      }
      gconf = gconf.split(/\r?\n/)
      var i = gconf.indexOf('[remote "origin"]')
      if (i !== -1) {
        var u = gconf[i + 1]
        if (!u.match(/^\s*url =/)) u = gconf[i + 2]
        if (!u.match(/^\s*url =/)) u = null
        else u = u.replace(/^\s*url = /, '')
      }
      if (u && u.match(/^git@github.com:/))
        u = u.replace(/^git@github.com:/, 'https://github.com/')

      return cb(null, yes ? u : prompt('git repository', u))
    })
  }
}

if (!package.keywords) {
  exports.keywords = yes ? '' : prompt('keywords', function (s) {
    if (!s) return undefined
    if (Array.isArray(s)) s = s.join(' ')
    if (typeof s !== 'string') return s
    return s.split(/[\s,]+/)
  })
}

if (!package.author) {
  exports.author = config.get('init.author.name') ||
                   config.get('init-author-name')
  ? {
      "name" : config.get('init.author.name') ||
               config.get('init-author-name'),
      "email" : config.get('init.author.email') ||
                config.get('init-author-email'),
      "url" : config.get('init.author.url') ||
              config.get('init-author-url')
    }
  : yes ? '' : prompt('author')
}

const defaultDottedInitLicense = config &&
  config.defaults &&
  config.defaults['init.license']
const dottedInitLicense =
  config.get('init.license') !== defaultDottedInitLicense &&
  config.get('init.license')
var license = package.license ||
              dottedInitLicense ||
              config.get('init-license') ||
              'ISC'
exports.license = yes ? license : prompt('license', license, function (data) {
  var its = validateLicense(data)
  if (its.validForNewPackages) return data
  var errors = (its.errors || []).concat(its.warnings || [])
  var er = new Error('Sorry, ' + errors.join(' and ') + '.')
  er.notValid = true
  return er
})
