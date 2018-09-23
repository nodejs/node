// npm version <newver>

module.exports = version

var semver = require('semver')
var path = require('path')
var fs = require('graceful-fs')
var writeFileAtomic = require('write-file-atomic')
var chain = require('slide').chain
var log = require('npmlog')
var npm = require('./npm.js')
var git = require('./utils/git.js')
var assert = require('assert')
var lifecycle = require('./utils/lifecycle.js')
var parseJSON = require('./utils/parse-json.js')

version.usage = 'npm version [<newversion> | major | minor | patch | premajor | preminor | prepatch | prerelease]' +
                '\n(run in package dir)\n' +
                "'npm -v' or 'npm --version' to print npm version " +
                '(' + npm.version + ')\n' +
                "'npm view <pkg> version' to view a package's " +
                'published version\n' +
                "'npm ls' to inspect current package/dependency versions"

function version (args, silent, cb_) {
  if (typeof cb_ !== 'function') {
    cb_ = silent
    silent = false
  }
  if (args.length > 1) return cb_(version.usage)

  var packagePath = path.join(npm.localPrefix, 'package.json')
  fs.readFile(packagePath, function (er, data) {
    if (data) data = data.toString()
    try {
      data = parseJSON(data)
    } catch (e) {
      er = e
      data = null
    }

    if (!args.length) return dump(data, cb_)

    if (er) {
      log.error('version', 'No valid package.json found')
      return cb_(er)
    }

    var newVersion = semver.valid(args[0])
    if (!newVersion) newVersion = semver.inc(data.version, args[0])
    if (!newVersion) return cb_(version.usage)
    if (data.version === newVersion) return cb_(new Error('Version not changed'))
    data.version = newVersion
    var lifecycleData = Object.create(data)
    lifecycleData._id = data.name + '@' + newVersion
    var localData = {}

    var where = npm.prefix
    chain([
          [checkGit, localData],
          [lifecycle, lifecycleData, 'preversion', where],
          [updatePackage, newVersion, silent],
          [lifecycle, lifecycleData, 'version', where],
          [commit, localData, newVersion],
          [lifecycle, lifecycleData, 'postversion', where] ],
          cb_)
  })
}

function readPackage (cb) {
  var packagePath = path.join(npm.localPrefix, 'package.json')
  fs.readFile(packagePath, function (er, data) {
    if (er) return cb(new Error(er))
    if (data) data = data.toString()
    try {
      data = JSON.parse(data)
    } catch (e) {
      er = e
      data = null
    }
    cb(er, data)
  })
}

function updatePackage (newVersion, silent, cb_) {
  function cb (er) {
    if (!er && !silent) console.log('v' + newVersion)
    cb_(er)
  }

  readPackage(function (er, data) {
    if (er) return cb(new Error(er))
    data.version = newVersion
    write(data, 'package.json', cb)
  })
}

function commit (localData, newVersion, cb) {
  updateShrinkwrap(newVersion, function (er, hasShrinkwrap) {
    if (er || !localData.hasGit) return cb(er)
    _commit(newVersion, hasShrinkwrap, cb)
  })
}

function updateShrinkwrap (newVersion, cb) {
  fs.readFile(path.join(npm.localPrefix, 'npm-shrinkwrap.json'), function (er, data) {
    if (er && er.code === 'ENOENT') return cb(null, false)

    try {
      data = data.toString()
      data = parseJSON(data)
    } catch (er) {
      log.error('version', 'Bad npm-shrinkwrap.json data')
      return cb(er)
    }

    data.version = newVersion
    write(data, 'npm-shrinkwrap.json', function (er) {
      if (er) {
        log.error('version', 'Bad npm-shrinkwrap.json data')
        return cb(er)
      }
      cb(null, true)
    })
  })
}

function dump (data, cb) {
  var v = {}

  if (data && data.name && data.version) v[data.name] = data.version
  v.npm = npm.version
  Object.keys(process.versions).sort().forEach(function (k) {
    v[k] = process.versions[k]
  })

  if (npm.config.get('json')) v = JSON.stringify(v, null, 2)

  console.log(v)
  cb()
}

function checkGit (localData, cb) {
  fs.stat(path.join(npm.localPrefix, '.git'), function (er, s) {
    var doGit = !er && npm.config.get('git-tag-version')
    if (!doGit) {
      if (er) log.verbose('version', 'error checking for .git', er)
      log.verbose('version', 'not tagging in git')
      return cb(null, false)
    }

    // check for git
    git.whichAndExec(
      [ 'status', '--porcelain' ],
      { env: process.env },
      function (er, stdout) {
        if (er && er.code === 'ENOGIT') {
          log.warn(
            'version',
            'This is a Git checkout, but the git command was not found.',
            'npm could not create a Git tag for this release!'
          )
          return cb(null, false)
        }

        var lines = stdout.trim().split('\n').filter(function (line) {
          return line.trim() && !line.match(/^\?\? /)
        }).map(function (line) {
          return line.trim()
        })
        if (lines.length && !npm.config.get('force')) {
          return cb(new Error(
            'Git working directory not clean.\n' + lines.join('\n')
          ))
        }
        localData.hasGit = true
        cb(null, true)
      }
    )
  })
}

function _commit (version, hasShrinkwrap, cb) {
  var options = { env: process.env }
  var message = npm.config.get('message').replace(/%s/g, version)
  var sign = npm.config.get('sign-git-tag')
  var flag = sign ? '-sm' : '-am'
  chain(
    [
      git.chainableExec([ 'add', 'package.json' ], options),
      hasShrinkwrap && git.chainableExec([ 'add', 'npm-shrinkwrap.json' ], options),
      git.chainableExec([ 'commit', '-m', message ], options),
      git.chainableExec([ 'tag', npm.config.get('tag-version-prefix') + version, flag, message ], options)
    ],
    cb
  )
}

function write (data, file, cb) {
  assert(data && typeof data === 'object', 'must pass data to version write')
  assert(typeof file === 'string', 'must pass filename to write to version write')

  log.verbose('version.write', 'data', data, 'to', file)
  writeFileAtomic(
    path.join(npm.localPrefix, file),
    new Buffer(JSON.stringify(data, null, 2) + '\n'),
    cb
  )
}
