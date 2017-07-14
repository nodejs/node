'use strict'
const BB = require('bluebird')

const assert = require('assert')
const chain = require('slide').chain
const detectIndent = require('detect-indent')
const fs = require('graceful-fs')
const readFile = BB.promisify(require('graceful-fs').readFile)
const git = require('./utils/git.js')
const lifecycle = require('./utils/lifecycle.js')
const log = require('npmlog')
const npm = require('./npm.js')
const output = require('./utils/output.js')
const parseJSON = require('./utils/parse-json.js')
const path = require('path')
const semver = require('semver')
const writeFileAtomic = require('write-file-atomic')

version.usage = 'npm version [<newversion> | major | minor | patch | premajor | preminor | prepatch | prerelease | from-git]' +
                '\n(run in package dir)\n' +
                "'npm -v' or 'npm --version' to print npm version " +
                '(' + npm.version + ')\n' +
                "'npm view <pkg> version' to view a package's " +
                'published version\n' +
                "'npm ls' to inspect current package/dependency versions"

// npm version <newver>
module.exports = version
function version (args, silent, cb_) {
  if (typeof cb_ !== 'function') {
    cb_ = silent
    silent = false
  }
  if (args.length > 1) return cb_(version.usage)

  readPackage(function (er, data, indent) {
    if (!args.length) return dump(data, cb_)

    if (er) {
      log.error('version', 'No valid package.json found')
      return cb_(er)
    }

    if (args[0] === 'from-git') {
      retrieveTagVersion(silent, data, cb_)
    } else {
      var newVersion = semver.valid(args[0])
      if (!newVersion) newVersion = semver.inc(data.version, args[0])
      if (!newVersion) return cb_(version.usage)
      persistVersion(newVersion, silent, data, cb_)
    }
  })
}

function retrieveTagVersion (silent, data, cb_) {
  chain([
    verifyGit,
    parseLastGitTag
  ], function (er, results) {
    if (er) return cb_(er)
    var localData = {
      hasGit: true,
      existingTag: true
    }

    var version = results[results.length - 1]
    persistVersion(version, silent, data, localData, cb_)
  })
}

function parseLastGitTag (cb) {
  var options = { env: process.env }
  git.whichAndExec(['describe', '--abbrev=0'], options, function (er, stdout) {
    if (er) {
      if (er.message.indexOf('No names found') !== -1) return cb(new Error('No tags found'))
      return cb(er)
    }

    var tag = stdout.trim()
    var prefix = npm.config.get('tag-version-prefix')
    // Strip the prefix from the start of the tag:
    if (tag.indexOf(prefix) === 0) tag = tag.slice(prefix.length)
    var version = semver.valid(tag)
    if (!version) return cb(new Error(tag + ' is not a valid version'))
    cb(null, version)
  })
}

function persistVersion (newVersion, silent, data, localData, cb_) {
  if (typeof localData === 'function') {
    cb_ = localData
    localData = {}
  }

  if (!npm.config.get('allow-same-version') && data.version === newVersion) {
    return cb_(new Error('Version not changed, might want --allow-same-version'))
  }
  data.version = newVersion
  var lifecycleData = Object.create(data)
  lifecycleData._id = data.name + '@' + newVersion

  var where = npm.prefix
  chain([
    !localData.hasGit && [checkGit, localData],
    [lifecycle, lifecycleData, 'preversion', where],
    [updatePackage, newVersion, silent],
    [lifecycle, lifecycleData, 'version', where],
    [commit, localData, newVersion],
    [lifecycle, lifecycleData, 'postversion', where]
  ], cb_)
}

function readPackage (cb) {
  var packagePath = path.join(npm.localPrefix, 'package.json')
  fs.readFile(packagePath, 'utf8', function (er, data) {
    if (er) return cb(new Error(er))
    var indent
    try {
      indent = detectIndent(data).indent || '  '
      data = JSON.parse(data)
    } catch (e) {
      er = e
      data = null
    }
    cb(er, data, indent)
  })
}

function updatePackage (newVersion, silent, cb_) {
  function cb (er) {
    if (!er && !silent) output('v' + newVersion)
    cb_(er)
  }

  readPackage(function (er, data, indent) {
    if (er) return cb(new Error(er))
    data.version = newVersion
    write(data, 'package.json', indent, cb)
  })
}

function commit (localData, newVersion, cb) {
  updateShrinkwrap(newVersion, function (er, hasShrinkwrap, hasLock) {
    if (er || !localData.hasGit) return cb(er)
    localData.hasShrinkwrap = hasShrinkwrap
    localData.hasPackageLock = hasLock
    _commit(newVersion, localData, cb)
  })
}

const SHRINKWRAP = 'npm-shrinkwrap.json'
const PKGLOCK = 'package-lock.json'

function readLockfile (name) {
  return readFile(
    path.join(npm.localPrefix, name), 'utf8'
  ).catch({code: 'ENOENT'}, () => null)
}

function updateShrinkwrap (newVersion, cb) {
  BB.join(
    readLockfile(SHRINKWRAP),
    readLockfile(PKGLOCK),
    (shrinkwrap, lockfile) => {
      if (!shrinkwrap && !lockfile) {
        return cb(null, false, false)
      }
      const file = shrinkwrap ? SHRINKWRAP : PKGLOCK
      let data
      let indent
      try {
        data = parseJSON(shrinkwrap || lockfile)
        indent = detectIndent(shrinkwrap || lockfile).indent || '  '
      } catch (err) {
        log.error('version', `Bad ${file} data.`)
        return cb(err)
      }
      data.version = newVersion
      write(data, file, indent, (err) => {
        if (err) {
          log.error('version', `Failed to update version in ${file}`)
          return cb(err)
        } else {
          return cb(null, !!shrinkwrap, !!lockfile)
        }
      })
    }
  )
}

function dump (data, cb) {
  var v = {}

  if (data && data.name && data.version) v[data.name] = data.version
  v.npm = npm.version
  Object.keys(process.versions).sort().forEach(function (k) {
    v[k] = process.versions[k]
  })

  if (npm.config.get('json')) v = JSON.stringify(v, null, 2)

  output(v)
  cb()
}

function statGitFolder (cb) {
  fs.stat(path.join(npm.localPrefix, '.git'), cb)
}

function callGitStatus (cb) {
  git.whichAndExec(
    [ 'status', '--porcelain' ],
    { env: process.env },
    cb
  )
}

function cleanStatusLines (stdout) {
  var lines = stdout.trim().split('\n').filter(function (line) {
    return line.trim() && !line.match(/^\?\? /)
  }).map(function (line) {
    return line.trim()
  })

  return lines
}

function verifyGit (cb) {
  function checkStatus (er) {
    if (er) return cb(er)
    callGitStatus(checkStdout)
  }

  function checkStdout (er, stdout) {
    if (er) return cb(er)
    var lines = cleanStatusLines(stdout)
    if (lines.length > 0) {
      return cb(new Error(
        'Git working directory not clean.\n' + lines.join('\n')
      ))
    }

    cb()
  }

  statGitFolder(checkStatus)
}

function checkGit (localData, cb) {
  statGitFolder(function (er) {
    var doGit = !er && npm.config.get('git-tag-version')
    if (!doGit) {
      if (er) log.verbose('version', 'error checking for .git', er)
      log.verbose('version', 'not tagging in git')
      return cb(null, false)
    }

    // check for git
    callGitStatus(function (er, stdout) {
      if (er && er.code === 'ENOGIT') {
        log.warn(
          'version',
          'This is a Git checkout, but the git command was not found.',
          'npm could not create a Git tag for this release!'
        )
        return cb(null, false)
      }

      var lines = cleanStatusLines(stdout)
      if (lines.length && !npm.config.get('force')) {
        return cb(new Error(
          'Git working directory not clean.\n' + lines.join('\n')
        ))
      }
      localData.hasGit = true
      cb(null, true)
    })
  })
}

function _commit (version, localData, cb) {
  const options = { env: process.env }
  const message = npm.config.get('message').replace(/%s/g, version)
  const sign = npm.config.get('sign-git-tag')
  const flagForTag = sign ? '-sm' : '-am'

  stagePackageFiles(localData, options).then(() => {
    return git.exec([ 'commit', '-m', message ], options)
  }).then(() => {
    if (!localData.existingTag) {
      return git.exec([
        'tag', npm.config.get('tag-version-prefix') + version,
        flagForTag, message
      ], options)
    }
  }).nodeify(cb)
}

function stagePackageFiles (localData, options) {
  return addLocalFile('package.json', options, false).then(() => {
    if (localData.hasShrinkwrap) {
      return addLocalFile('npm-shrinkwrap.json', options, false)
    } else if (localData.hasPackageLock) {
      return addLocalFile('package-lock.json', options, false)
    }
  })
}

function addLocalFile (file, options, ignoreFailure) {
  const p = git.exec(['add', path.join(npm.localPrefix, file)], options)
  return ignoreFailure
  ? p.catch(() => {})
  : p
}

function write (data, file, indent, cb) {
  assert(data && typeof data === 'object', 'must pass data to version write')
  assert(typeof file === 'string', 'must pass filename to write to version write')

  log.verbose('version.write', 'data', data, 'to', file)
  writeFileAtomic(
    path.join(npm.localPrefix, file),
    new Buffer(JSON.stringify(data, null, indent || 2) + '\n'),
    cb
  )
}
