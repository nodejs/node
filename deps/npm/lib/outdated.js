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
var log = require('npmlog')
var readPackageTree = require('read-package-tree')
var readJson = require('read-package-json')
var asyncMap = require('slide').asyncMap
var color = require('ansicolors')
var styles = require('ansistyles')
var table = require('text-table')
var semver = require('semver')
var npa = require('npm-package-arg')
var mutateIntoLogicalTree = require('./install/mutate-into-logical-tree.js')
var cache = require('./cache.js')
var npm = require('./npm.js')
var long = npm.config.get('long')
var mapToRegistry = require('./utils/map-to-registry.js')
var isExtraneous = require('./install/is-extraneous.js')
var recalculateMetadata = require('./install/deps.js').recalculateMetadata
var moduleName = require('./utils/module-name.js')

function uniqName (item) {
  return item[0].path + '|' + item[1] + '|' + item[7]
}

function uniq (list) {
  var uniqed = []
  var seen = {}
  list.forEach(function (item) {
    var name = uniqName(item)
    if (seen[name]) return
    seen[name] = true
    uniqed.push(item)
  })
  return uniqed
}

function andRecalculateMetadata (next) {
  return function (er, tree) {
    if (er) return next(er)
    recalculateMetadata(tree, log, next)
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

  readPackageTree(dir, andRecalculateMetadata(function (er, tree) {
    mutateIntoLogicalTree(tree)
    outdated_(args, '', tree, {}, 0, function (er, list) {
      list = uniq(list || []).sort(function (aa, bb) {
        return aa[0].path.localeCompare(bb[0].path) ||
          aa[1].localeCompare(bb[1])
      })
      if (er || silent || list.length === 0) return cb(er, list)
      log.disableProgress()
      if (npm.config.get('json')) {
        console.log(makeJSON(list))
      } else if (npm.config.get('parseable')) {
        console.log(makeParseable(list))
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
        console.log(table(outTable, tableOpts))
      }
      cb(null, list.map(function (item) { return [item[0].parent.path].concat(item.slice(1, 7)) }))
    })
  }))
}

// [[ dir, dep, has, want, latest, type ]]
function makePretty (p) {
  var dep = p[0]
  var depname = p[1]
  var dir = dep.path
  var has = p[2]
  var want = p[3]
  var latest = p[4]
  var type = p[6]
  var deppath = p[7]

  if (!npm.config.get('global')) {
    dir = path.relative(process.cwd(), dir)
  }

  var columns = [ depname,
                  has || 'MISSING',
                  want,
                  latest,
                  deppath
                ]
  if (long) columns[5] = type

  if (npm.color) {
    columns[0] = color[has === want ? 'yellow' : 'red'](columns[0]) // dep
    columns[2] = color.green(columns[2]) // want
    columns[3] = color.magenta(columns[3]) // latest
    columns[4] = color.brightBlack(columns[4]) // dir
    if (long) columns[5] = color.brightBlack(columns[5]) // type
  }

  return columns
}

function ansiTrim (str) {
  var r = new RegExp('\x1b(?:\\[(?:\\d+[ABCDEFGJKSTm]|\\d+;\\d+[Hfm]|' +
        '\\d+;\\d+;\\d+m|6n|s|u|\\?25[lh])|\\w)', 'g')
  return str.replace(r, '')
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

  var deps = tree.children.filter(function (child) { return !isExtraneous(child) }) || []

  deps.forEach(function (dep) {
    types[moduleName(dep)] = 'dependencies'
  })

  Object.keys(tree.missingDeps).forEach(function (name) {
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
    Object.keys(pkg.devDependencies).forEach(function (k) {
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
      version: child.package.version,
      from: child.package._from
    }
  })

  // now get what we should have, based on the dep.
  // if has[dep] !== shouldHave[dep], then cb with the data
  // otherwise dive into the folder
  asyncMap(deps, function (dep, cb) {
    var name = moduleName(dep)
    var required = (tree.package.dependencies)[name] ||
                   (tree.package.optionalDependencies)[name] ||
                   (tree.package.devDependencies)[name] ||
                   dep.package._requested && dep.package._requested.spec ||
                   '*'
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
  var parsed = npa(dep + '@' + req)
  if (tree.isLink && (tree.parent !== null && tree.parent.parent === null)) {
    return doIt('linked', 'linked')
  }
  if (parsed.type === 'git' || parsed.type === 'hosted') {
    return doIt('git', 'git')
  }

  // search for the latest package
  mapToRegistry(dep, npm.config, function (er, uri, auth) {
    if (er) return cb(er)

    npm.registry.get(uri, { auth: auth }, updateDeps)
  })

  function updateLocalDeps (latestRegistryVersion) {
    readJson(path.resolve(parsed.spec, 'package.json'), function (er, localDependency) {
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

      if (curr.version !== wanted) {
        doIt(wanted, latest)
      } else {
        skip()
      }
    })
  }

  function updateDeps (er, d) {
    if (er) {
      if (parsed.type !== 'local') return cb(er)
      return updateLocalDeps()
    }

    if (!d || !d['dist-tags'] || !d.versions) return cb()
    var l = d.versions[d['dist-tags'].latest]
    if (!l) return cb()

    var r = req
    if (d['dist-tags'][req]) {
      r = d['dist-tags'][req]
    }

    if (semver.validRange(r, true)) {
      // some kind of semver range.
      // see if it's in the doc.
      var vers = Object.keys(d.versions)
      var v = semver.maxSatisfying(vers, r, true)
      if (v) {
        return onCacheAdd(null, d.versions[v])
      }
    }

    // We didn't find the version in the doc.  See if cache can find it.
    cache.add(dep, req, null, false, onCacheAdd)

    function onCacheAdd (er, d) {
      // if this fails, then it means we can't update this thing.
      // it's probably a thing that isn't published.
      if (er) {
        if (er.code && er.code === 'ETARGET') {
          // no viable version found
          return skip(er)
        }
        return skip()
      }

      // check that the url origin hasn't changed (#1727) and that
      // there is no newer version available
      var dFromUrl = d._from && url.parse(d._from).protocol
      var cFromUrl = curr && curr.from && url.parse(curr.from).protocol

      if (!curr ||
          dFromUrl && cFromUrl && d._from !== curr.from ||
          d.version !== curr.version ||
          d.version !== l.version) {
        if (parsed.type === 'local') return updateLocalDeps(l.version)

        doIt(d.version, l.version)
      } else {
        skip()
      }
    }
  }
}
