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

const os = require('os')
const url = require('url')
const path = require('path')
const readPackageTree = require('read-package-tree')
const asyncMap = require('slide').asyncMap
const color = require('ansicolors')
const styles = require('ansistyles')
const table = require('text-table')
const semver = require('semver')
const npa = require('libnpm/parse-arg')
const pickManifest = require('npm-pick-manifest')
const fetchPackageMetadata = require('./fetch-package-metadata.js')
const mutateIntoLogicalTree = require('./install/mutate-into-logical-tree.js')
const npm = require('./npm.js')
const npmConfig = require('./config/figgy-config.js')
const figgyPudding = require('figgy-pudding')
const packument = require('libnpm/packument')
const long = npm.config.get('long')
const isExtraneous = require('./install/is-extraneous.js')
const computeMetadata = require('./install/deps.js').computeMetadata
const computeVersionSpec = require('./install/deps.js').computeVersionSpec
const moduleName = require('./utils/module-name.js')
const output = require('./utils/output.js')
const ansiTrim = require('./utils/ansi-trim')

const OutdatedConfig = figgyPudding({
  also: {},
  color: {},
  depth: {},
  dev: 'development',
  development: {},
  global: {},
  json: {},
  only: {},
  parseable: {},
  prod: 'production',
  production: {},
  save: {},
  'save-dev': {},
  'save-optional': {}
})

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
  let opts = OutdatedConfig(npmConfig())
  var dir = path.resolve(npm.dir, '..')

  // default depth for `outdated` is 0 (cf. `ls`)
  if (opts.depth === Infinity) opts = opts.concat({depth: 0})

  readPackageTree(dir, andComputeMetadata(function (er, tree) {
    if (!tree) return cb(er)
    mutateIntoLogicalTree(tree)
    outdated_(args, '', tree, {}, 0, opts, function (er, list) {
      list = uniq(list || []).sort(function (aa, bb) {
        return aa[0].path.localeCompare(bb[0].path) ||
          aa[1].localeCompare(bb[1])
      })
      if (er || silent ||
          (list.length === 0 && !opts.json)) {
        return cb(er, list)
      }
      if (opts.json) {
        output(makeJSON(list, opts))
      } else if (opts.parseable) {
        output(makeParseable(list, opts))
      } else {
        var outList = list.map(x => makePretty(x, opts))
        var outHead = [ 'Package',
          'Current',
          'Wanted',
          'Latest',
          'Location'
        ]
        if (long) outHead.push('Package Type', 'Homepage')
        var outTable = [outHead].concat(outList)

        if (opts.color) {
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
      process.exitCode = list.length ? 1 : 0
      cb(null, list.map(function (item) { return [item[0].parent.path].concat(item.slice(1, 7)) }))
    })
  }))
}

// [[ dir, dep, has, want, latest, type ]]
function makePretty (p, opts) {
  var depname = p[1]
  var has = p[2]
  var want = p[3]
  var latest = p[4]
  var type = p[6]
  var deppath = p[7]
  var homepage = p[0].package.homepage || ''

  var columns = [ depname,
    has || 'MISSING',
    want,
    latest,
    deppath || 'global'
  ]
  if (long) {
    columns[5] = type
    columns[6] = homepage
  }

  if (opts.color) {
    columns[0] = color[has === want ? 'yellow' : 'red'](columns[0]) // dep
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
    if (long) out.push(type, dep.package.homepage)

    return out.join(':')
  }).join(os.EOL)
}

function makeJSON (list, opts) {
  var out = {}
  list.forEach(function (p) {
    var dep = p[0]
    var depname = p[1]
    var dir = dep.path
    var has = p[2]
    var want = p[3]
    var latest = p[4]
    var type = p[6]
    if (!opts.global) {
      dir = path.relative(process.cwd(), dir)
    }
    out[depname] = { current: has,
      wanted: want,
      latest: latest,
      location: dir
    }
    if (long) {
      out[depname].type = type
      out[depname].homepage = dep.package.homepage
    }
  })
  return JSON.stringify(out, null, 2)
}

function outdated_ (args, path, tree, parentHas, depth, opts, cb) {
  if (!tree.package) tree.package = {}
  if (path && moduleName(tree)) path += ' > ' + tree.package.name
  if (!path && moduleName(tree)) path = tree.package.name
  if (depth > opts.depth) {
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
  var dev = opts.dev || /^dev(elopment)?$/.test(opts.also)
  var prod = opts.production || /^prod(uction)?$/.test(opts.only)
  if (
    (dev || !prod) &&
    (
      opts['save-dev'] || (!opts.save && !opts['save-optional'])
    )
  ) {
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

  if (opts['save-dev']) {
    deps = deps.filter(function (dep) { return pkg.devDependencies[moduleName(dep)] })
    deps.forEach(function (dep) {
      types[moduleName(dep)] = 'devDependencies'
    })
  } else if (opts.save) {
    // remove optional dependencies from dependencies during --save.
    deps = deps.filter(function (dep) { return !pkg.optionalDependencies[moduleName(dep)] })
  } else if (opts['save-optional']) {
    deps = deps.filter(function (dep) { return pkg.optionalDependencies[moduleName(dep)] })
    deps.forEach(function (dep) {
      types[moduleName(dep)] = 'optionalDependencies'
    })
  }
  var doUpdate = dev || (
    !prod &&
    !Object.keys(parentHas).length &&
    !opts.global
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
    if (moduleName(child) && child.package.private) {
      deps = deps.filter(function (dep) { return dep !== child })
    }
    has[moduleName(child)] = {
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

    if (!long) return shouldUpdate(args, dep, name, has, required, depth, path, opts, cb)

    shouldUpdate(args, dep, name, has, required, depth, path, opts, cb, types[name])
  }, cb)
}

function shouldUpdate (args, tree, dep, has, req, depth, pkgpath, opts, cb, type) {
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
      opts,
      cb)
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
  } else if (parsed.type === 'remote') {
    return doIt('remote', 'remote')
  } else {
    return packument(parsed, opts.concat({
      'prefer-online': true
    })).nodeify(updateDeps)
  }

  function doIt (wanted, latest) {
    let c = curr && curr.version
    if (parsed.type === 'alias') {
      c = `npm:${parsed.subSpec.name}@${c}`
    }
    if (!long) {
      return cb(null, [[tree, dep, c, wanted, latest, req, null, pkgpath]])
    }
    cb(null, [[tree, dep, c, wanted, latest, req, type, pkgpath]])
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

    if (parsed.type === 'alias') {
      req = parsed.subSpec.rawSpec
    }
    try {
      var l = pickManifest(d, 'latest')
      var m = pickManifest(d, req)
    } catch (er) {
      if (er.code === 'ETARGET' || er.code === 'E403') {
        return skip(er)
      } else {
        return skip()
      }
    }

    // check that the url origin hasn't changed (#1727) and that
    // there is no newer version available
    var dFromUrl = m._from && url.parse(m._from).protocol
    var cFromUrl = curr && curr.from && url.parse(curr.from).protocol

    if (
      !curr ||
      (dFromUrl && cFromUrl && m._from !== curr.from) ||
      m.version !== curr.version ||
      m.version !== l.version
    ) {
      if (parsed.type === 'alias') {
        doIt(
          `npm:${parsed.subSpec.name}@${m.version}`,
          `npm:${parsed.subSpec.name}@${l.version}`
        )
      } else {
        doIt(m.version, l.version)
      }
    } else {
      skip()
    }
  }
}
