// show the installed versions of packages
//
// --parseable creates output like this:
// <fullpath>:<name@ver>:<realpath>:<flags>
// Flags are a :-separated list of zero or more indicators

module.exports = exports = ls

var path = require('path')
var url = require('url')
var readPackageTree = require('read-package-tree')
var log = require('npmlog')
var archy = require('archy')
var semver = require('semver')
var color = require('ansicolors')
var npa = require('npm-package-arg')
var iferr = require('iferr')
var npm = require('./npm.js')
var mutateIntoLogicalTree = require('./install/mutate-into-logical-tree.js')
var recalculateMetadata = require('./install/deps.js').recalculateMetadata
var packageId = require('./utils/package-id.js')
var usage = require('./utils/usage')

ls.usage = usage(
  'ls',
  'npm ls [[<@scope>/]<pkg> ...]'
)

ls.completion = require('./utils/completion/installed-deep.js')

function ls (args, silent, cb) {
  if (typeof cb !== 'function') {
    cb = silent
    silent = false
  }
  var dir = path.resolve(npm.dir, '..')
  readPackageTree(dir, andRecalculateMetadata(iferr(cb, function (physicalTree) {
    lsFromTree(dir, physicalTree, args, silent, cb)
  })))
}

function andRecalculateMetadata (next) {
  return function (er, tree) {
    recalculateMetadata(tree || {}, log, next)
  }
}

function inList (list, value) {
  return list.indexOf(value) !== -1
}

var lsFromTree = ls.fromTree = function (dir, physicalTree, args, silent, cb) {
  if (typeof cb !== 'function') {
    cb = silent
    silent = false
  }

  // npm ls 'foo@~1.3' bar 'baz@<2'
  if (!args) {
    args = []
  } else {
    args = args.map(function (a) {
      var p = npa(a)
      var name = p.name
      // When version spec is missing, we'll skip using it when filtering.
      // Otherwise, `semver.validRange` would return '*', which won't
      // match prerelease versions.
      var ver = (p.rawSpec &&
                 (semver.validRange(p.rawSpec) || ''))
      return [ name, ver, a ]
    })
  }

  var data = mutateIntoLogicalTree.asReadInstalled(physicalTree)

  pruneNestedExtraneous(data)
  filterByEnv(data)
  var bfs = filterFound(bfsify(data), args)
  var lite = getLite(bfs)

  if (silent) return cb(null, data, lite)

  var long = npm.config.get('long')
  var json = npm.config.get('json')
  var out
  if (json) {
    var seen = []
    var d = long ? bfs : lite
    // the raw data can be circular
    out = JSON.stringify(d, function (k, o) {
      if (typeof o === 'object') {
        if (inList(seen, o)) return '[Circular]'
        seen.push(o)
      }
      return o
    }, 2)
  } else if (npm.config.get('parseable')) {
    out = makeParseable(bfs, long, dir)
  } else if (data) {
    out = makeArchy(bfs, long, dir)
  }
  console.log(out)

  if (args.length && !data._found) process.exitCode = 1

  var er
  // if any errors were found, then complain and exit status 1
  if (lite.problems && lite.problems.length) {
    er = lite.problems.join('\n')
  }
  cb(er, data, lite)
}

function pruneNestedExtraneous (data, visited) {
  visited = visited || []
  visited.push(data)
  for (var i in data.dependencies) {
    if (data.dependencies[i].extraneous) {
      data.dependencies[i].dependencies = {}
    } else if (visited.indexOf(data.dependencies[i]) === -1) {
      pruneNestedExtraneous(data.dependencies[i], visited)
    }
  }
}

function filterByEnv (data) {
  var dev = npm.config.get('dev') || /^dev(elopment)?$/.test(npm.config.get('only'))
  var production = npm.config.get('production') || /^prod(uction)?$/.test(npm.config.get('only'))
  var dependencies = {}
  var devKeys = Object.keys(data.devDependencies || [])
  var prodKeys = Object.keys(data._dependencies || [])
  Object.keys(data.dependencies).forEach(function (name) {
    if (!dev && inList(devKeys, name) && data.dependencies[name].missing) {
      return
    }

    if ((dev && inList(devKeys, name)) ||            // only --dev
        (production && inList(prodKeys, name)) ||    // only --production
        (!dev && !production)) {                            // no --production|--dev|--only=xxx
      dependencies[name] = data.dependencies[name]
    }
  })
  data.dependencies = dependencies
}

function alphasort (a, b) {
  a = a.toLowerCase()
  b = b.toLowerCase()
  return a > b ? 1
       : a < b ? -1 : 0
}

function isCruft (data) {
  return data.extraneous && data.error && data.error.code === 'ENOTDIR'
}

function getLite (data, noname, depth) {
  var lite = {}

  if (isCruft(data)) return lite

  var maxDepth = npm.config.get('depth')

  if (typeof depth === 'undefined') depth = 0
  if (!noname && data.name) lite.name = data.name
  if (data.version) lite.version = data.version
  if (data.extraneous) {
    lite.extraneous = true
    lite.problems = lite.problems || []
    lite.problems.push('extraneous: ' + packageId(data) + ' ' + (data.path || ''))
  }

  if (data.error && data.path !== path.resolve(npm.globalDir, '..') &&
      (data.error.code !== 'ENOENT' || noname)) {
    lite.invalid = true
    lite.problems = lite.problems || []
    var message = data.error.message
    lite.problems.push('error in ' + data.path + ': ' + message)
  }

  if (data._from) {
    lite.from = data._from
  }

  if (data._resolved) {
    lite.resolved = data._resolved
  }

  if (data.invalid) {
    lite.invalid = true
    lite.problems = lite.problems || []
    lite.problems.push('invalid: ' +
                       packageId(data) +
                       ' ' + (data.path || ''))
  }

  if (data.peerInvalid) {
    lite.peerInvalid = true
    lite.problems = lite.problems || []
    lite.problems.push('peer dep not met: ' +
                       packageId(data) +
                       ' ' + (data.path || ''))
  }

  var deps = (data.dependencies && Object.keys(data.dependencies)) || []
  if (deps.length) {
    lite.dependencies = deps.map(function (d) {
      var dep = data.dependencies[d]
      if (dep.missing && !dep.optional) {
        lite.problems = lite.problems || []
        var p
        if (data.depth > maxDepth) {
          p = 'max depth reached: '
        } else {
          p = 'missing: '
        }
        p += d + '@' + dep.requiredBy +
            ', required by ' +
            packageId(data)
        lite.problems.push(p)
        return [d, { required: dep.requiredBy, missing: true }]
      } else if (dep.peerMissing) {
        lite.problems = lite.problems || []
        dep.peerMissing.forEach(function (missing) {
          var pdm = 'peer dep missing: ' +
              missing.requires +
              ', required by ' +
              missing.requiredBy
          lite.problems.push(pdm)
        })
        return [d, { required: dep, peerMissing: true }]
      } else if (npm.config.get('json')) {
        if (depth === maxDepth) delete dep.dependencies
        return [d, getLite(dep, true, depth + 1)]
      }
      return [d, getLite(dep, true)]
    }).reduce(function (deps, d) {
      if (d[1].problems) {
        lite.problems = lite.problems || []
        lite.problems.push.apply(lite.problems, d[1].problems)
      }
      deps[d[0]] = d[1]
      return deps
    }, {})
  }
  return lite
}

function bfsify (root) {
  // walk over the data, and turn it from this:
  // +-- a
  // |   `-- b
  // |       `-- a (truncated)
  // `--b (truncated)
  // into this:
  // +-- a
  // `-- b
  // which looks nicer
  var queue = [root]
  var seen = [root]

  while (queue.length) {
    var current = queue.shift()
    var deps = current.dependencies = current.dependencies || {}
    Object.keys(deps).forEach(function (d) {
      var dep = deps[d]
      if (dep.missing) return
      if (inList(seen, dep)) {
        if (npm.config.get('parseable') || !npm.config.get('long')) {
          delete deps[d]
          return
        } else {
          dep = deps[d] = Object.create(dep)
          dep.dependencies = {}
        }
      }
      queue.push(dep)
      seen.push(dep)
    })
  }

  return root
}

function filterFound (root, args) {
  if (!args.length) return root
  var deps = root.dependencies
  if (deps) {
    Object.keys(deps).forEach(function (depName) {
      var dep = filterFound(deps[depName], args)
      if (dep.peerMissing) return

      // see if this one itself matches
      var found = false
      for (var ii = 0; !found && ii < args.length; ii++) {
        var argName = args[ii][0]
        var argVersion = args[ii][1]
        var argRaw = args[ii][2]
        if (depName === argName && argVersion) {
          found = semver.satisfies(dep.version, argVersion, true)
        } else if (depName === argName) {
          // If version is missing from arg, just do a name match.
          found = true
        } else if (dep.path === argRaw) {
          found = true
        }
      }
      // included explicitly
      if (found) dep._found = true
      // included because a child was included
      if (dep._found && !root._found) root._found = 1
      // not included
      if (!dep._found) delete deps[depName]
    })
  }
  if (!root._found) root._found = false
  return root
}

function makeArchy (data, long, dir) {
  var out = makeArchy_(data, long, dir, 0)
  return archy(out, '', { unicode: npm.config.get('unicode') })
}

function makeArchy_ (data, long, dir, depth, parent, d) {
  if (data.missing) {
    if (depth - 1 <= npm.config.get('depth')) {
      // just missing
      var unmet = 'UNMET ' + (data.optional ? 'OPTIONAL ' : '') + 'DEPENDENCY'
      if (npm.color) {
        if (data.optional) {
          unmet = color.bgBlack(color.yellow(unmet))
        } else {
          unmet = color.bgBlack(color.red(unmet))
        }
      }
      data = unmet + ' ' + d + '@' + data.requiredBy
    } else {
      data = d + '@' + data.requiredBy
    }
    return data
  }

  var out = {}
  // the top level is a bit special.
  out.label = data._id || ''
  if (data._found === true && data._id) {
    if (npm.color) {
      out.label = color.bgBlack(color.yellow(out.label.trim())) + ' '
    } else {
      out.label = out.label.trim() + ' '
    }
  }
  if (data.link) out.label += ' -> ' + data.link

  if (data.invalid) {
    if (data.realName !== data.name) out.label += ' (' + data.realName + ')'
    var invalid = 'invalid'
    if (npm.color) invalid = color.bgBlack(color.red(invalid))
    out.label += ' ' + invalid
  }

  if (data.peerInvalid) {
    var peerInvalid = 'peer invalid'
    if (npm.color) peerInvalid = color.bgBlack(color.red(peerInvalid))
    out.label += ' ' + peerInvalid
  }

  if (data.peerMissing) {
    var peerMissing = 'UNMET PEER DEPENDENCY'
    if (npm.color) peerMissing = color.bgBlack(color.red(peerMissing))
    out.label = peerMissing + ' ' + out.label
  }

  if (data.extraneous && data.path !== dir) {
    var extraneous = 'extraneous'
    if (npm.color) extraneous = color.bgBlack(color.green(extraneous))
    out.label += ' ' + extraneous
  }

  if (data.error && depth) {
    var message = data.error.message
    if (message.indexOf('\n')) message = message.slice(0, message.indexOf('\n'))
    var error = 'error: ' + message
    if (npm.color) error = color.bgRed(color.brightWhite(error))
    out.label += ' ' + error
  }

  // add giturl to name@version
  if (data._resolved) {
    try {
      var type = npa(data._resolved).type
      var isGit = type === 'git' || type === 'hosted'
      if (isGit) {
        out.label += ' (' + data._resolved + ')'
      }
    } catch (ex) {
      // npa threw an exception then it ain't git so whatev
    }
  }

  if (long) {
    if (dir === data.path) out.label += '\n' + dir
    out.label += '\n' + getExtras(data, dir)
  } else if (dir === data.path) {
    if (out.label) out.label += ' '
    out.label += dir
  }

  // now all the children.
  out.nodes = []
  if (depth <= npm.config.get('depth')) {
    out.nodes = Object.keys(data.dependencies || {})
      .sort(alphasort).filter(function (d) {
        return !isCruft(data.dependencies[d])
      }).map(function (d) {
        return makeArchy_(data.dependencies[d], long, dir, depth + 1, data, d)
      })
  }

  if (out.nodes.length === 0 && data.path === dir) {
    out.nodes = ['(empty)']
  }

  return out
}

function getExtras (data) {
  var extras = []

  if (data.description) extras.push(data.description)
  if (data.repository) extras.push(data.repository.url)
  if (data.homepage) extras.push(data.homepage)
  if (data._from) {
    var from = data._from
    if (from.indexOf(data.name + '@') === 0) {
      from = from.substr(data.name.length + 1)
    }
    var u = url.parse(from)
    if (u.protocol) extras.push(from)
  }
  return extras.join('\n')
}

function makeParseable (data, long, dir, depth, parent, d) {
  depth = depth || 0
  if (depth > npm.config.get('depth')) return [ makeParseable_(data, long, dir, depth, parent, d) ]
  return [ makeParseable_(data, long, dir, depth, parent, d) ]
  .concat(Object.keys(data.dependencies || {})
    .sort(alphasort).map(function (d) {
      return makeParseable(data.dependencies[d], long, dir, depth + 1, data, d)
    }))
  .filter(function (x) { return x })
  .join('\n')
}

function makeParseable_ (data, long, dir, depth, parent, d) {
  if (data.hasOwnProperty('_found') && data._found !== true) return ''

  if (data.missing) {
    if (depth < npm.config.get('depth')) {
      data = npm.config.get('long')
           ? path.resolve(parent.path, 'node_modules', d) +
             ':' + d + '@' + JSON.stringify(data.requiredBy) + ':INVALID:MISSING'
           : ''
    } else {
      data = path.resolve(dir || '', 'node_modules', d || '') +
             (npm.config.get('long')
             ? ':' + d + '@' + JSON.stringify(data.requiredBy) +
               ':' + // no realpath resolved
               ':MAXDEPTH'
             : '')
    }

    return data
  }

  if (!npm.config.get('long')) return data.path

  return data.path +
         ':' + (data._id || '') +
         ':' + (data.realPath !== data.path ? data.realPath : '') +
         (data.extraneous ? ':EXTRANEOUS' : '') +
         (data.error && data.path !== path.resolve(npm.globalDir, '..') ? ':ERROR' : '') +
         (data.invalid ? ':INVALID' : '') +
         (data.peerInvalid ? ':PEERINVALID' : '') +
         (data.peerMissing ? ':PEERINVALID:MISSING' : '')
}
