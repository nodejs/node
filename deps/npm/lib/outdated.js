/*

npm outdated [pkg]

Does the following:

1. check for a new version of pkg

If no packages are specified, then run for all installed
packages.

--parseable creates output like this:
<fullpath>:<name@wanted>:<name@installed>:<name@latest>

*/

module.exports = outdated

outdated.usage = 'npm outdated [[<@scope>/]<pkg> ...]'

outdated.completion = require('./utils/completion/installed-deep.js')

var os = require('os')
var url = require('url')
var path = require('path')
var readPackageTree = require('read-package-tree')
var asyncMap = require('slide').asyncMap
var color = require('ansicolors')
var styles = require('ansistyles')
var table = require('text-table')
var semver = require('semver')
var npa = require('npm-package-arg')
var pickManifest = require('npm-pick-manifest')
var fetchPackageMetadata = require('./fetch-package-metadata.js')
var mutateIntoLogicalTree = require('./install/mutate-into-logical-tree.js')
var npm = require('./npm.js')
var long = npm.config.get('long')
var mapToRegistry = require('./utils/map-to-registry.js')
var isExtraneous = require('./install/is-extraneous.js')
var computeMetadata = require('./install/deps.js').computeMetadata
var computeVersionSpec = require('./install/deps.js').computeVersionSpec
var moduleName = require('./utils/module-name.js')
var output = require('./utils/output.js')
var ansiTrim = require('./utils/ansi-trim')

function uniq (list) {
  // we maintain the array because we need an array, not iterator, return
  // value.
  var uniqed = []
  var seen = new Set()
  list.forEach(function (item) {
    if (seen.has(item)) return
    seen.add(item)
    uniqed.push(item)
  })
  return uniqed
}

function andComputeMetadata (next) {
  return function (er, tree) {
    if (er) return next(er)
    next(null, computeMetadata(tree))
  }
}

function outdated (args, silent, cb) {
  if (typeof cb !== 'function') {
    cb = silent
    silent = false
  }
  var dir = path.resolve(npm.dir, '..')

  // default depth for `outdated` is 0 (cf. `ls`)
  if (npm.config.get('depth') === Infinity) npm.config.set('depth', 0)

  readPackageTree(dir, andComputeMetadata(function (er, tree) {
    if (!tree) return cb(er)
    mutateIntoLogicalTree(tree)
    outdated_(args, '', tree, {}, 0, function (er, list) {
      list = uniq(list || []).sort(function (aa, bb) {
        return aa[0].path.localeCompare(bb[0].path) ||
          aa[1].localeCompare(bb[1])
      })
      if (er || silent || list.length === 0) return cb(er, list)
      if (npm.config.get('json')) {
        output(makeJSON(list))
      } else if (npm.config.get('parseable')) {
        output(makeParseable(list))
      } else {
        var outList = list.map(makePretty)
        var outHead = [ 'Package',
          'Current',
          'Wanted',
          'Latest',
          'Location'
        ]
        if (long) outHead.push('Package Type')
        var outTable = [outHead].concat(outList)

        if (npm.color) {
          outTable[0] = outTable[0].map(function (heading) {
            return styles.underline(heading)
          })
        }

        var tableOpts = {
          align: ['l', 'r', 'r', 'r', 'l'],
          stringLength: function (s) { return ansiTrim(s).length }
        }
        output(table(outTable, tableOpts))
      }
      process.exitCode = 1
      cb(null, list.map(function (item) { return [item[0].parent.path].concat(item.slice(1, 7)) }))
    })
  }))
}

// [[ dir, dep, has, want, latest, type ]]
function makePretty (p) {
  var depname = p[1]
  var has = p[2]
  var want = p[3]
  var latest = p[4]
  var type = p[6]
  var deppath = p[7]

  var columns = [ depname,
    has || 'MISSING',
    want,
    latest,
    deppath
  ]
  if (long) columns[5] = type

  if (npm.color) {
    columns[0] = color[has === want || want === 'linked' ? 'yellow' : 'red'](columns[0]) // dep
    columns[2] = color.green(columns[2]) // want
    columns[3] = color.magenta(columns[3]) // latest
  }

  return columns
}

function makeParseable (list) {
  return list.map(function (p) {
    var dep = p[0]
    var depname = p[1]
    var dir = dep.path
    var has = p[2]
    var want = p[3]
    var latest = p[4]
    var type = p[6]

    var out = [
      dir,
      depname + '@' + want,
      (has ? (depname + '@' + has) : 'MISSING'),
      depname + '@' + latest
    ]
    if (long) out.push(type)

    return out.join(':')
  }).join(os.EOL)
}

function makeJSON (list) {
  var out = {}
  list.forEach(function (p) {
    var dep = p[0]
    var depname = p[1]
    var dir = dep.path
    var has = p[2]
    var want = p[3]
    var latest = p[4]
    var type = p[6]
    if (!npm.config.get('global')) {
      dir = path.relative(process.cwd(), dir)
    }
    out[depname] = { current: has,
      wanted: want,
      latest: latest,
      location: dir
    }
    if (long) out[depname].type = type
  })
  return JSON.stringify(out, null, 2)
}

function outdated_ (args, path, tree, parentHas, depth, cb) {
  if (!tree.package) tree.package = {}
  if (path && tree.package.name) path += ' > ' + tree.package.name
  if (!path && tree.package.name) path = tree.package.name
  if (depth > npm.config.get('depth')) {
    return cb(null, [])
  }
  var types = {}
  var pkg = tree.package

  if (!tree.children) tree.children = []

  var deps = tree.error ? tree.children : tree.children.filter((child) => !isExtraneous(child))

  deps.forEach(function (dep) {
    types[moduleName(dep)] = 'dependencies'
  })

  Object.keys(tree.missingDeps || {}).forEach(function (name) {
    deps.push({
      package: { name: name },
      path: tree.path,
      parent: tree,
      isMissing: true
    })
    types[name] = 'dependencies'
  })

  // If we explicitly asked for dev deps OR we didn't ask for production deps
  // AND we asked to save dev-deps OR we didn't ask to save anything that's NOT
  // dev deps then…
  // (All the save checking here is because this gets called from npm-update currently
  // and that requires this logic around dev deps.)
  // FIXME: Refactor npm update to not be in terms of outdated.
  var dev = npm.config.get('dev') || /^dev(elopment)?$/.test(npm.config.get('also'))
  var prod = npm.config.get('production') || /^prod(uction)?$/.test(npm.config.get('only'))
  if ((dev || !prod) &&
      (npm.config.get('save-dev') || (
        !npm.config.get('save') && !npm.config.get('save-optional')))) {
    Object.keys(tree.missingDevDeps).forEach(function (name) {
      deps.push({
        package: { name: name },
        path: tree.path,
        parent: tree,
        isMissing: true
      })
      if (!types[name]) {
        types[name] = 'devDependencies'
      }
    })
  }

  if (npm.config.get('save-dev')) {
    deps = deps.filter(function (dep) { return pkg.devDependencies[moduleName(dep)] })
    deps.forEach(function (dep) {
      types[moduleName(dep)] = 'devDependencies'
    })
  } else if (npm.config.get('save')) {
    // remove optional dependencies from dependencies during --save.
    deps = deps.filter(function (dep) { return !pkg.optionalDependencies[moduleName(dep)] })
  } else if (npm.config.get('save-optional')) {
    deps = deps.filter(function (dep) { return pkg.optionalDependencies[moduleName(dep)] })
    deps.forEach(function (dep) {
      types[moduleName(dep)] = 'optionalDependencies'
    })
  }
  var doUpdate = dev || (
    !prod &&
    !Object.keys(parentHas).length &&
    !npm.config.get('global')
  )
  if (doUpdate) {
    Object.keys(pkg.devDependencies || {}).forEach(function (k) {
      if (!(k in parentHas)) {
        deps[k] = pkg.devDependencies[k]
        types[k] = 'devDependencies'
      }
    })
  }

  var has = Object.create(parentHas)
  tree.children.forEach(function (child) {
    if (child.package.name && child.package.private) {
      deps = deps.filter(function (dep) { return dep !== child })
    }
    has[child.package.name] = {
      version: child.isLink ? 'linked' : child.package.version,
      from: child.isLink ? 'file:' + child.path : child.package._from
    }
  })

  // now get what we should have, based on the dep.
  // if has[dep] !== shouldHave[dep], then cb with the data
  // otherwise dive into the folder
  asyncMap(deps, function (dep, cb) {
    var name = moduleName(dep)
    var required
    if (tree.package.dependencies && name in tree.package.dependencies) {
      required = tree.package.dependencies[name]
    } else if (tree.package.optionalDependencies && name in tree.package.optionalDependencies) {
      required = tree.package.optionalDependencies[name]
    } else if (tree.package.devDependencies && name in tree.package.devDependencies) {
      required = tree.package.devDependencies[name]
    } else if (has[name]) {
      required = computeVersionSpec(tree, dep)
    }

    if (!long) return shouldUpdate(args, dep, name, has, required, depth, path, cb)

    shouldUpdate(args, dep, name, has, required, depth, path, cb, types[name])
  }, cb)
}

function shouldUpdate (args, tree, dep, has, req, depth, pkgpath, cb, type) {
  // look up the most recent version.
  // if that's what we already have, or if it's not on the args list,
  // then dive into it.  Otherwise, cb() with the data.

  // { version: , from: }
  var curr = has[dep]

  function skip (er) {
    // show user that no viable version can be found
    if (er) return cb(er)
    outdated_(args,
      pkgpath,
      tree,
      has,
      depth + 1,
      cb)
  }

  function doIt (wanted, latest) {
    if (!long) {
      return cb(null, [[tree, dep, curr && curr.version, wanted, latest, req, null, pkgpath]])
    }
    cb(null, [[tree, dep, curr && curr.version, wanted, latest, req, type, pkgpath]])
  }

  if (args.length && args.indexOf(dep) === -1) return skip()

  if (tree.isLink && req == null) return skip()

  if (req == null || req === '') req = '*'

  var parsed = npa.resolve(dep, req)
  if (parsed.type === 'directory') {
    if (tree.isLink) {
      return skip()
    } else {
      return doIt('linked', 'linked')
    }
  } else if (parsed.type === 'git') {
    return doIt('git', 'git')
  } else if (parsed.type === 'file') {
    return updateLocalDeps()
  } else {
    return mapToRegistry(dep, npm.config, function (er, uri, auth) {
      if (er) return cb(er)

      npm.registry.get(uri, { auth: auth }, updateDeps)
    })
  }

  function updateLocalDeps (latestRegistryVersion) {
    fetchPackageMetadata('file:' + parsed.fetchSpec, '.', (er, localDependency) => {
      if (er) return cb()

      var wanted = localDependency.version
      var latest = localDependency.version

      if (latestRegistryVersion) {
        latest = latestRegistryVersion
        if (semver.lt(wanted, latestRegistryVersion)) {
          wanted = latestRegistryVersion
          req = dep + '@' + latest
        }
      }

      if (!curr || curr.version !== wanted) {
        doIt(wanted, latest)
      } else {
        skip()
      }
    })
  }

  function updateDeps (er, d) {
    if (er) return cb(er)

    try {
      var l = pickManifest(d, 'latest')
      var m = pickManifest(d, req)
    } catch (er) {
      if (er.code === 'ETARGET') {
        return skip(er)
      } else {
        return skip()
      }
    }

    // check that the url origin hasn't changed (#1727) and that
    // there is no newer version available
    var dFromUrl = m._from && url.parse(m._from).protocol
    var cFromUrl = curr && curr.from && url.parse(curr.from).protocol

    if (!curr ||
        (dFromUrl && cFromUrl && m._from !== curr.from) ||
        m.version !== curr.version ||
        m.version !== l.version) {
      doIt(m.version, l.version)
    } else {
      skip()
    }
  }
}
