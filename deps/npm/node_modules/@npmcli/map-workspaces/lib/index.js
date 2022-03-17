const { promisify } = require('util')
const path = require('path')

const getName = require('@npmcli/name-from-folder')
const minimatch = require('minimatch')
const rpj = require('read-package-json-fast')
const glob = require('glob')
const pGlob = promisify(glob)

function appendNegatedPatterns (patterns) {
  const results = []
  for (let pattern of patterns) {
    const excl = pattern.match(/^!+/)
    if (excl) {
      pattern = pattern.substr(excl[0].length)
    }

    // strip off any / from the start of the pattern.  /foo => foo
    pattern = pattern.replace(/^\/+/, '')

    // an odd number of ! means a negated pattern.  !!foo ==> foo
    const negate = excl && excl[0].length % 2 === 1
    results.push({ pattern, negate })
  }

  return results
}

function getPatterns (workspaces) {
  const workspacesDeclaration =
    Array.isArray(workspaces.packages)
      ? workspaces.packages
      : workspaces

  if (!Array.isArray(workspacesDeclaration)) {
    throw getError({
      message: 'workspaces config expects an Array',
      code: 'EWORKSPACESCONFIG',
    })
  }

  return appendNegatedPatterns(workspacesDeclaration)
}

function getPackageName (pkg, pathname) {
  const { name } = pkg
  return name || getName(pathname)
}

function pkgPathmame (opts) {
  return (...args) => {
    const cwd = opts.cwd ? opts.cwd : process.cwd()
    return path.join.apply(null, [cwd, ...args])
  }
}

// make sure glob pattern only matches folders
function getGlobPattern (pattern) {
  pattern = pattern.replace(/\\/g, '/')
  return pattern.endsWith('/')
    ? pattern
    : `${pattern}/`
}

function getError ({ Type = TypeError, message, code }) {
  return Object.assign(new Type(message), { code })
}

function reverseResultMap (map) {
  return new Map(Array.from(map, item => item.reverse()))
}

async function mapWorkspaces (opts = {}) {
  if (!opts || !opts.pkg) {
    throw getError({
      message: 'mapWorkspaces missing pkg info',
      code: 'EMAPWORKSPACESPKG',
    })
  }

  const { workspaces = [] } = opts.pkg
  const patterns = getPatterns(workspaces)
  const results = new Map()
  const seen = new Map()

  if (!patterns.length) {
    return results
  }

  const getGlobOpts = () => ({
    ...opts,
    ignore: [
      ...opts.ignore || [],
      ...['**/node_modules/**'],
    ],
  })

  const getPackagePathname = pkgPathmame(opts)

  for (const item of patterns) {
    const matches = await pGlob(getGlobPattern(item.pattern), getGlobOpts())

    for (const match of matches) {
      let pkg
      const packageJsonPathname = getPackagePathname(match, 'package.json')
      const packagePathname = path.dirname(packageJsonPathname)

      try {
        pkg = await rpj(packageJsonPathname)
      } catch (err) {
        if (err.code === 'ENOENT') {
          continue
        } else {
          throw err
        }
      }

      const name = getPackageName(pkg, packagePathname)

      let seenPackagePathnames = seen.get(name)
      if (!seenPackagePathnames) {
        seenPackagePathnames = new Set()
        seen.set(name, seenPackagePathnames)
      }
      if (item.negate) {
        seenPackagePathnames.delete(packagePathname)
      } else {
        seenPackagePathnames.add(packagePathname)
      }
    }
  }

  const errorMessageArray = ['must not have multiple workspaces with the same name']
  for (const [packageName, seenPackagePathnames] of seen) {
    if (seenPackagePathnames.size === 0) {
      continue
    }
    if (seenPackagePathnames.size > 1) {
      addDuplicateErrorMessages(errorMessageArray, packageName, seenPackagePathnames)
    } else {
      results.set(packageName, seenPackagePathnames.values().next().value)
    }
  }

  if (errorMessageArray.length > 1) {
    throw getError({
      Type: Error,
      message: errorMessageArray.join('\n'),
      code: 'EDUPLICATEWORKSPACE',
    })
  }

  return results
}

function addDuplicateErrorMessages (messageArray, packageName, packagePathnames) {
  messageArray.push(
    `package '${packageName}' has conflicts in the following paths:`
  )

  for (const packagePathname of packagePathnames) {
    messageArray.push(
      '    ' + packagePathname
    )
  }
}

mapWorkspaces.virtual = function (opts = {}) {
  if (!opts || !opts.lockfile) {
    throw getError({
      message: 'mapWorkspaces.virtual missing lockfile info',
      code: 'EMAPWORKSPACESLOCKFILE',
    })
  }

  const { packages = {} } = opts.lockfile
  const { workspaces = [] } = packages[''] || {}
  // uses a pathname-keyed map in order to negate the exact items
  const results = new Map()
  const patterns = getPatterns(workspaces)
  if (!patterns.length) {
    return results
  }
  patterns.push({ pattern: '**/node_modules/**', negate: true })

  const getPackagePathname = pkgPathmame(opts)

  for (const packageKey of Object.keys(packages)) {
    if (packageKey === '') {
      continue
    }

    for (const item of patterns) {
      if (minimatch(packageKey, item.pattern)) {
        const packagePathname = getPackagePathname(packageKey)
        const name = getPackageName(packages[packageKey], packagePathname)

        if (item.negate) {
          results.delete(packagePathname)
        } else {
          results.set(packagePathname, name)
        }
      }
    }
  }

  // Invert pathname-keyed to a proper name-to-pathnames Map
  return reverseResultMap(results)
}

module.exports = mapWorkspaces
