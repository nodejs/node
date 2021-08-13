// Manually “tree shaken” from:
// <https://github.com/nodejs/node/blob/89f592c/lib/internal/modules/esm/resolve.js>

/**
 * @typedef {'module'|'commonjs'|'none'} PackageType
 *
 * @typedef PackageConfig
 * @property {string} pjsonPath
 * @property {boolean} exists
 * @property {string} main
 * @property {string} name
 * @property {PackageType} type
 * @property {Object.<string, unknown>} exports
 * @property {Object.<string, unknown>} imports
 *
 * @typedef ResolveObject
 * @property {URL} resolved
 * @property {boolean} exact
 */

import {URL, fileURLToPath, pathToFileURL} from 'url'
import {Stats, statSync, realpathSync} from 'fs'
import path from 'path'
import builtins from 'builtins'
import packageJsonReader from './package-json-reader.js'
import {defaultGetFormat} from './get-format.js'
import {codes} from './errors.js'

const listOfBuiltins = builtins()

const {
  ERR_INVALID_MODULE_SPECIFIER,
  ERR_INVALID_PACKAGE_CONFIG,
  ERR_INVALID_PACKAGE_TARGET,
  ERR_MODULE_NOT_FOUND,
  ERR_PACKAGE_IMPORT_NOT_DEFINED,
  ERR_PACKAGE_PATH_NOT_EXPORTED,
  ERR_UNSUPPORTED_DIR_IMPORT,
  ERR_UNSUPPORTED_ESM_URL_SCHEME,
  ERR_INVALID_ARG_VALUE
} = codes

const own = {}.hasOwnProperty

const DEFAULT_CONDITIONS = Object.freeze(['node', 'import'])
const DEFAULT_CONDITIONS_SET = new Set(DEFAULT_CONDITIONS)

const invalidSegmentRegEx = /(^|\\|\/)(\.\.?|node_modules)(\\|\/|$)/
const patternRegEx = /\*/g
const encodedSepRegEx = /%2f|%2c/i
/** @type {Set<string>} */
const emittedPackageWarnings = new Set()
/** @type {Map<string, PackageConfig>} */
const packageJsonCache = new Map()

/**
 * @param {string} match
 * @param {URL} pjsonUrl
 * @param {boolean} isExports
 * @param {URL} base
 * @returns {void}
 */
function emitFolderMapDeprecation(match, pjsonUrl, isExports, base) {
  const pjsonPath = fileURLToPath(pjsonUrl)

  if (emittedPackageWarnings.has(pjsonPath + '|' + match)) return
  emittedPackageWarnings.add(pjsonPath + '|' + match)
  process.emitWarning(
    `Use of deprecated folder mapping "${match}" in the ${
      isExports ? '"exports"' : '"imports"'
    } field module resolution of the package at ${pjsonPath}${
      base ? ` imported from ${fileURLToPath(base)}` : ''
    }.\n` +
      `Update this package.json to use a subpath pattern like "${match}*".`,
    'DeprecationWarning',
    'DEP0148'
  )
}

/**
 * @param {URL} url
 * @param {URL} packageJsonUrl
 * @param {URL} base
 * @param {unknown} [main]
 * @returns {void}
 */
function emitLegacyIndexDeprecation(url, packageJsonUrl, base, main) {
  const {format} = defaultGetFormat(url.href)
  if (format !== 'module') return
  const path = fileURLToPath(url.href)
  const pkgPath = fileURLToPath(new URL('.', packageJsonUrl))
  const basePath = fileURLToPath(base)
  if (main)
    process.emitWarning(
      `Package ${pkgPath} has a "main" field set to ${JSON.stringify(main)}, ` +
        `excluding the full filename and extension to the resolved file at "${path.slice(
          pkgPath.length
        )}", imported from ${basePath}.\n Automatic extension resolution of the "main" field is` +
        'deprecated for ES modules.',
      'DeprecationWarning',
      'DEP0151'
    )
  else
    process.emitWarning(
      `No "main" or "exports" field defined in the package.json for ${pkgPath} resolving the main entry point "${path.slice(
        pkgPath.length
      )}", imported from ${basePath}.\nDefault "index" lookups for the main are deprecated for ES modules.`,
      'DeprecationWarning',
      'DEP0151'
    )
}

/**
 * @param {string[]} [conditions]
 * @returns {Set<string>}
 */
function getConditionsSet(conditions) {
  if (conditions !== undefined && conditions !== DEFAULT_CONDITIONS) {
    if (!Array.isArray(conditions)) {
      throw new ERR_INVALID_ARG_VALUE(
        'conditions',
        conditions,
        'expected an array'
      )
    }

    return new Set(conditions)
  }

  return DEFAULT_CONDITIONS_SET
}

/**
 * @param {string} path
 * @returns {Stats}
 */
function tryStatSync(path) {
  // Note: from Node 15 onwards we can use `throwIfNoEntry: false` instead.
  try {
    return statSync(path)
  } catch {
    return new Stats()
  }
}

/**
 * @param {string} path
 * @param {string|URL} specifier Note: `specifier` is actually optional, not base.
 * @param {URL} [base]
 * @returns {PackageConfig}
 */
function getPackageConfig(path, specifier, base) {
  const existing = packageJsonCache.get(path)
  if (existing !== undefined) {
    return existing
  }

  const source = packageJsonReader.read(path).string

  if (source === undefined) {
    /** @type {PackageConfig} */
    const packageConfig = {
      pjsonPath: path,
      exists: false,
      main: undefined,
      name: undefined,
      type: 'none',
      exports: undefined,
      imports: undefined
    }
    packageJsonCache.set(path, packageConfig)
    return packageConfig
  }

  /** @type {Object.<string, unknown>} */
  let packageJson
  try {
    packageJson = JSON.parse(source)
  } catch (error) {
    throw new ERR_INVALID_PACKAGE_CONFIG(
      path,
      (base ? `"${specifier}" from ` : '') + fileURLToPath(base || specifier),
      error.message
    )
  }

  const {exports, imports, main, name, type} = packageJson

  /** @type {PackageConfig} */
  const packageConfig = {
    pjsonPath: path,
    exists: true,
    main: typeof main === 'string' ? main : undefined,
    name: typeof name === 'string' ? name : undefined,
    type: type === 'module' || type === 'commonjs' ? type : 'none',
    // @ts-expect-error Assume `Object.<string, unknown>`.
    exports,
    // @ts-expect-error Assume `Object.<string, unknown>`.
    imports: imports && typeof imports === 'object' ? imports : undefined
  }
  packageJsonCache.set(path, packageConfig)
  return packageConfig
}

/**
 * @param {URL|string} resolved
 * @returns {PackageConfig}
 */
function getPackageScopeConfig(resolved) {
  let packageJsonUrl = new URL('./package.json', resolved)

  while (true) {
    const packageJsonPath = packageJsonUrl.pathname

    if (packageJsonPath.endsWith('node_modules/package.json')) break

    const packageConfig = getPackageConfig(
      fileURLToPath(packageJsonUrl),
      resolved
    )
    if (packageConfig.exists) return packageConfig

    const lastPackageJsonUrl = packageJsonUrl
    packageJsonUrl = new URL('../package.json', packageJsonUrl)

    // Terminates at root where ../package.json equals ../../package.json
    // (can't just check "/package.json" for Windows support).
    if (packageJsonUrl.pathname === lastPackageJsonUrl.pathname) break
  }

  const packageJsonPath = fileURLToPath(packageJsonUrl)
  /** @type {PackageConfig} */
  const packageConfig = {
    pjsonPath: packageJsonPath,
    exists: false,
    main: undefined,
    name: undefined,
    type: 'none',
    exports: undefined,
    imports: undefined
  }
  packageJsonCache.set(packageJsonPath, packageConfig)
  return packageConfig
}

/**
 * Legacy CommonJS main resolution:
 * 1. let M = pkg_url + (json main field)
 * 2. TRY(M, M.js, M.json, M.node)
 * 3. TRY(M/index.js, M/index.json, M/index.node)
 * 4. TRY(pkg_url/index.js, pkg_url/index.json, pkg_url/index.node)
 * 5. NOT_FOUND
 *
 * @param {URL} url
 * @returns {boolean}
 */
function fileExists(url) {
  return tryStatSync(fileURLToPath(url)).isFile()
}

/**
 * @param {URL} packageJsonUrl
 * @param {PackageConfig} packageConfig
 * @param {URL} base
 * @returns {URL}
 */
function legacyMainResolve(packageJsonUrl, packageConfig, base) {
  /** @type {URL} */
  let guess
  if (packageConfig.main !== undefined) {
    guess = new URL(`./${packageConfig.main}`, packageJsonUrl)
    // Note: fs check redundances will be handled by Descriptor cache here.
    if (fileExists(guess)) return guess

    const tries = [
      `./${packageConfig.main}.js`,
      `./${packageConfig.main}.json`,
      `./${packageConfig.main}.node`,
      `./${packageConfig.main}/index.js`,
      `./${packageConfig.main}/index.json`,
      `./${packageConfig.main}/index.node`
    ]
    let i = -1

    while (++i < tries.length) {
      guess = new URL(tries[i], packageJsonUrl)
      if (fileExists(guess)) break
      guess = undefined
    }

    if (guess) {
      emitLegacyIndexDeprecation(
        guess,
        packageJsonUrl,
        base,
        packageConfig.main
      )
      return guess
    }
    // Fallthrough.
  }

  const tries = ['./index.js', './index.json', './index.node']
  let i = -1

  while (++i < tries.length) {
    guess = new URL(tries[i], packageJsonUrl)
    if (fileExists(guess)) break
    guess = undefined
  }

  if (guess) {
    emitLegacyIndexDeprecation(guess, packageJsonUrl, base, packageConfig.main)
    return guess
  }

  // Not found.
  throw new ERR_MODULE_NOT_FOUND(
    fileURLToPath(new URL('.', packageJsonUrl)),
    fileURLToPath(base)
  )
}

/**
 * @param {URL} resolved
 * @param {URL} base
 * @returns {URL}
 */
function finalizeResolution(resolved, base) {
  if (encodedSepRegEx.test(resolved.pathname))
    throw new ERR_INVALID_MODULE_SPECIFIER(
      resolved.pathname,
      'must not include encoded "/" or "\\" characters',
      fileURLToPath(base)
    )

  const path = fileURLToPath(resolved)

  const stats = tryStatSync(path.endsWith('/') ? path.slice(-1) : path)

  if (stats.isDirectory()) {
    const error = new ERR_UNSUPPORTED_DIR_IMPORT(path, fileURLToPath(base))
    // @ts-expect-error Add this for `import.meta.resolve`.
    error.url = String(resolved)
    throw error
  }

  if (!stats.isFile()) {
    throw new ERR_MODULE_NOT_FOUND(
      path || resolved.pathname,
      base && fileURLToPath(base),
      'module'
    )
  }

  return resolved
}

/**
 * @param {string} specifier
 * @param {URL?} packageJsonUrl
 * @param {URL} base
 * @returns {never}
 */
function throwImportNotDefined(specifier, packageJsonUrl, base) {
  throw new ERR_PACKAGE_IMPORT_NOT_DEFINED(
    specifier,
    packageJsonUrl && fileURLToPath(new URL('.', packageJsonUrl)),
    fileURLToPath(base)
  )
}

/**
 * @param {string} subpath
 * @param {URL} packageJsonUrl
 * @param {URL} base
 * @returns {never}
 */
function throwExportsNotFound(subpath, packageJsonUrl, base) {
  throw new ERR_PACKAGE_PATH_NOT_EXPORTED(
    fileURLToPath(new URL('.', packageJsonUrl)),
    subpath,
    base && fileURLToPath(base)
  )
}

/**
 * @param {string} subpath
 * @param {URL} packageJsonUrl
 * @param {boolean} internal
 * @param {URL} [base]
 * @returns {never}
 */
function throwInvalidSubpath(subpath, packageJsonUrl, internal, base) {
  const reason = `request is not a valid subpath for the "${
    internal ? 'imports' : 'exports'
  }" resolution of ${fileURLToPath(packageJsonUrl)}`

  throw new ERR_INVALID_MODULE_SPECIFIER(
    subpath,
    reason,
    base && fileURLToPath(base)
  )
}

/**
 * @param {string} subpath
 * @param {unknown} target
 * @param {URL} packageJsonUrl
 * @param {boolean} internal
 * @param {URL} [base]
 * @returns {never}
 */
function throwInvalidPackageTarget(
  subpath,
  target,
  packageJsonUrl,
  internal,
  base
) {
  target =
    typeof target === 'object' && target !== null
      ? JSON.stringify(target, null, '')
      : `${target}`

  throw new ERR_INVALID_PACKAGE_TARGET(
    fileURLToPath(new URL('.', packageJsonUrl)),
    subpath,
    target,
    internal,
    base && fileURLToPath(base)
  )
}

/**
 * @param {string} target
 * @param {string} subpath
 * @param {string} match
 * @param {URL} packageJsonUrl
 * @param {URL} base
 * @param {boolean} pattern
 * @param {boolean} internal
 * @param {Set<string>} conditions
 * @returns {URL}
 */
function resolvePackageTargetString(
  target,
  subpath,
  match,
  packageJsonUrl,
  base,
  pattern,
  internal,
  conditions
) {
  if (subpath !== '' && !pattern && target[target.length - 1] !== '/')
    throwInvalidPackageTarget(match, target, packageJsonUrl, internal, base)

  if (!target.startsWith('./')) {
    if (internal && !target.startsWith('../') && !target.startsWith('/')) {
      let isURL = false

      try {
        new URL(target)
        isURL = true
      } catch {}

      if (!isURL) {
        const exportTarget = pattern
          ? target.replace(patternRegEx, subpath)
          : target + subpath

        return packageResolve(exportTarget, packageJsonUrl, conditions)
      }
    }

    throwInvalidPackageTarget(match, target, packageJsonUrl, internal, base)
  }

  if (invalidSegmentRegEx.test(target.slice(2)))
    throwInvalidPackageTarget(match, target, packageJsonUrl, internal, base)

  const resolved = new URL(target, packageJsonUrl)
  const resolvedPath = resolved.pathname
  const packagePath = new URL('.', packageJsonUrl).pathname

  if (!resolvedPath.startsWith(packagePath))
    throwInvalidPackageTarget(match, target, packageJsonUrl, internal, base)

  if (subpath === '') return resolved

  if (invalidSegmentRegEx.test(subpath))
    throwInvalidSubpath(match + subpath, packageJsonUrl, internal, base)

  if (pattern) return new URL(resolved.href.replace(patternRegEx, subpath))
  return new URL(subpath, resolved)
}

/**
 * @param {string} key
 * @returns {boolean}
 */
function isArrayIndex(key) {
  const keyNumber = Number(key)
  if (`${keyNumber}` !== key) return false
  return keyNumber >= 0 && keyNumber < 0xffff_ffff
}

/**
 * @param {URL} packageJsonUrl
 * @param {unknown} target
 * @param {string} subpath
 * @param {string} packageSubpath
 * @param {URL} base
 * @param {boolean} pattern
 * @param {boolean} internal
 * @param {Set<string>} conditions
 * @returns {URL}
 */
function resolvePackageTarget(
  packageJsonUrl,
  target,
  subpath,
  packageSubpath,
  base,
  pattern,
  internal,
  conditions
) {
  if (typeof target === 'string') {
    return resolvePackageTargetString(
      target,
      subpath,
      packageSubpath,
      packageJsonUrl,
      base,
      pattern,
      internal,
      conditions
    )
  }

  if (Array.isArray(target)) {
    /** @type {unknown[]} */
    const targetList = target
    if (targetList.length === 0) return null

    /** @type {Error} */
    let lastException
    let i = -1

    while (++i < targetList.length) {
      const targetItem = targetList[i]
      /** @type {URL} */
      let resolved
      try {
        resolved = resolvePackageTarget(
          packageJsonUrl,
          targetItem,
          subpath,
          packageSubpath,
          base,
          pattern,
          internal,
          conditions
        )
      } catch (error) {
        lastException = error
        if (error.code === 'ERR_INVALID_PACKAGE_TARGET') continue
        throw error
      }

      if (resolved === undefined) continue

      if (resolved === null) {
        lastException = null
        continue
      }

      return resolved
    }

    if (lastException === undefined || lastException === null) {
      // @ts-expect-error The diff between `undefined` and `null` seems to be
      // intentional
      return lastException
    }

    throw lastException
  }

  if (typeof target === 'object' && target !== null) {
    const keys = Object.getOwnPropertyNames(target)
    let i = -1

    while (++i < keys.length) {
      const key = keys[i]
      if (isArrayIndex(key)) {
        throw new ERR_INVALID_PACKAGE_CONFIG(
          fileURLToPath(packageJsonUrl),
          base,
          '"exports" cannot contain numeric property keys.'
        )
      }
    }

    i = -1

    while (++i < keys.length) {
      const key = keys[i]
      if (key === 'default' || (conditions && conditions.has(key))) {
        /** @type {unknown} */
        const conditionalTarget = target[key]
        const resolved = resolvePackageTarget(
          packageJsonUrl,
          conditionalTarget,
          subpath,
          packageSubpath,
          base,
          pattern,
          internal,
          conditions
        )
        if (resolved === undefined) continue
        return resolved
      }
    }

    return undefined
  }

  if (target === null) {
    return null
  }

  throwInvalidPackageTarget(
    packageSubpath,
    target,
    packageJsonUrl,
    internal,
    base
  )
}

/**
 * @param {unknown} exports
 * @param {URL} packageJsonUrl
 * @param {URL} base
 * @returns {boolean}
 */
function isConditionalExportsMainSugar(exports, packageJsonUrl, base) {
  if (typeof exports === 'string' || Array.isArray(exports)) return true
  if (typeof exports !== 'object' || exports === null) return false

  const keys = Object.getOwnPropertyNames(exports)
  let isConditionalSugar = false
  let i = 0
  let j = -1
  while (++j < keys.length) {
    const key = keys[j]
    const curIsConditionalSugar = key === '' || key[0] !== '.'
    if (i++ === 0) {
      isConditionalSugar = curIsConditionalSugar
    } else if (isConditionalSugar !== curIsConditionalSugar) {
      throw new ERR_INVALID_PACKAGE_CONFIG(
        fileURLToPath(packageJsonUrl),
        base,
        '"exports" cannot contain some keys starting with \'.\' and some not.' +
          ' The exports object must either be an object of package subpath keys' +
          ' or an object of main entry condition name keys only.'
      )
    }
  }

  return isConditionalSugar
}

/**
 * @param {URL} packageJsonUrl
 * @param {string} packageSubpath
 * @param {Object.<string, unknown>} packageConfig
 * @param {URL} base
 * @param {Set<string>} conditions
 * @returns {ResolveObject}
 */
function packageExportsResolve(
  packageJsonUrl,
  packageSubpath,
  packageConfig,
  base,
  conditions
) {
  let exports = packageConfig.exports
  if (isConditionalExportsMainSugar(exports, packageJsonUrl, base))
    exports = {'.': exports}

  if (own.call(exports, packageSubpath)) {
    const target = exports[packageSubpath]
    const resolved = resolvePackageTarget(
      packageJsonUrl,
      target,
      '',
      packageSubpath,
      base,
      false,
      false,
      conditions
    )
    if (resolved === null || resolved === undefined)
      throwExportsNotFound(packageSubpath, packageJsonUrl, base)
    return {resolved, exact: true}
  }

  let bestMatch = ''
  const keys = Object.getOwnPropertyNames(exports)
  let i = -1

  while (++i < keys.length) {
    const key = keys[i]
    if (
      key[key.length - 1] === '*' &&
      packageSubpath.startsWith(key.slice(0, -1)) &&
      packageSubpath.length >= key.length &&
      key.length > bestMatch.length
    ) {
      bestMatch = key
    } else if (
      key[key.length - 1] === '/' &&
      packageSubpath.startsWith(key) &&
      key.length > bestMatch.length
    ) {
      bestMatch = key
    }
  }

  if (bestMatch) {
    const target = exports[bestMatch]
    const pattern = bestMatch[bestMatch.length - 1] === '*'
    const subpath = packageSubpath.slice(bestMatch.length - (pattern ? 1 : 0))
    const resolved = resolvePackageTarget(
      packageJsonUrl,
      target,
      subpath,
      bestMatch,
      base,
      pattern,
      false,
      conditions
    )
    if (resolved === null || resolved === undefined)
      throwExportsNotFound(packageSubpath, packageJsonUrl, base)
    if (!pattern)
      emitFolderMapDeprecation(bestMatch, packageJsonUrl, true, base)
    return {resolved, exact: pattern}
  }

  throwExportsNotFound(packageSubpath, packageJsonUrl, base)
}

/**
 * @param {string} name
 * @param {URL} base
 * @param {Set<string>} [conditions]
 * @returns {ResolveObject}
 */
function packageImportsResolve(name, base, conditions) {
  if (name === '#' || name.startsWith('#/')) {
    const reason = 'is not a valid internal imports specifier name'
    throw new ERR_INVALID_MODULE_SPECIFIER(name, reason, fileURLToPath(base))
  }

  /** @type {URL} */
  let packageJsonUrl

  const packageConfig = getPackageScopeConfig(base)

  if (packageConfig.exists) {
    packageJsonUrl = pathToFileURL(packageConfig.pjsonPath)
    const imports = packageConfig.imports
    if (imports) {
      if (own.call(imports, name)) {
        const resolved = resolvePackageTarget(
          packageJsonUrl,
          imports[name],
          '',
          name,
          base,
          false,
          true,
          conditions
        )
        if (resolved !== null) return {resolved, exact: true}
      } else {
        let bestMatch = ''
        const keys = Object.getOwnPropertyNames(imports)
        let i = -1

        while (++i < keys.length) {
          const key = keys[i]

          if (
            key[key.length - 1] === '*' &&
            name.startsWith(key.slice(0, -1)) &&
            name.length >= key.length &&
            key.length > bestMatch.length
          ) {
            bestMatch = key
          } else if (
            key[key.length - 1] === '/' &&
            name.startsWith(key) &&
            key.length > bestMatch.length
          ) {
            bestMatch = key
          }
        }

        if (bestMatch) {
          const target = imports[bestMatch]
          const pattern = bestMatch[bestMatch.length - 1] === '*'
          const subpath = name.slice(bestMatch.length - (pattern ? 1 : 0))
          const resolved = resolvePackageTarget(
            packageJsonUrl,
            target,
            subpath,
            bestMatch,
            base,
            pattern,
            true,
            conditions
          )
          if (resolved !== null) {
            if (!pattern)
              emitFolderMapDeprecation(bestMatch, packageJsonUrl, false, base)
            return {resolved, exact: pattern}
          }
        }
      }
    }
  }

  throwImportNotDefined(name, packageJsonUrl, base)
}

/**
 * @param {string} url
 * @returns {PackageType}
 */
export function getPackageType(url) {
  const packageConfig = getPackageScopeConfig(url)
  return packageConfig.type
}

/**
 * @param {string} specifier
 * @param {URL} base
 */
function parsePackageName(specifier, base) {
  let separatorIndex = specifier.indexOf('/')
  let validPackageName = true
  let isScoped = false
  if (specifier[0] === '@') {
    isScoped = true
    if (separatorIndex === -1 || specifier.length === 0) {
      validPackageName = false
    } else {
      separatorIndex = specifier.indexOf('/', separatorIndex + 1)
    }
  }

  const packageName =
    separatorIndex === -1 ? specifier : specifier.slice(0, separatorIndex)

  // Package name cannot have leading . and cannot have percent-encoding or
  // separators.
  let i = -1
  while (++i < packageName.length) {
    if (packageName[i] === '%' || packageName[i] === '\\') {
      validPackageName = false
      break
    }
  }

  if (!validPackageName) {
    throw new ERR_INVALID_MODULE_SPECIFIER(
      specifier,
      'is not a valid package name',
      fileURLToPath(base)
    )
  }

  const packageSubpath =
    '.' + (separatorIndex === -1 ? '' : specifier.slice(separatorIndex))

  return {packageName, packageSubpath, isScoped}
}

/**
 * @param {string} specifier
 * @param {URL} base
 * @param {Set<string>} conditions
 * @returns {URL}
 */
function packageResolve(specifier, base, conditions) {
  const {packageName, packageSubpath, isScoped} = parsePackageName(
    specifier,
    base
  )

  // ResolveSelf
  const packageConfig = getPackageScopeConfig(base)

  // Can’t test.
  /* c8 ignore next 16 */
  if (packageConfig.exists) {
    const packageJsonUrl = pathToFileURL(packageConfig.pjsonPath)
    if (
      packageConfig.name === packageName &&
      packageConfig.exports !== undefined &&
      packageConfig.exports !== null
    ) {
      return packageExportsResolve(
        packageJsonUrl,
        packageSubpath,
        packageConfig,
        base,
        conditions
      ).resolved
    }
  }

  let packageJsonUrl = new URL(
    './node_modules/' + packageName + '/package.json',
    base
  )
  let packageJsonPath = fileURLToPath(packageJsonUrl)
  /** @type {string} */
  let lastPath
  do {
    const stat = tryStatSync(packageJsonPath.slice(0, -13))
    if (!stat.isDirectory()) {
      lastPath = packageJsonPath
      packageJsonUrl = new URL(
        (isScoped ? '../../../../node_modules/' : '../../../node_modules/') +
          packageName +
          '/package.json',
        packageJsonUrl
      )
      packageJsonPath = fileURLToPath(packageJsonUrl)
      continue
    }

    // Package match.
    const packageConfig = getPackageConfig(packageJsonPath, specifier, base)
    if (packageConfig.exports !== undefined && packageConfig.exports !== null)
      return packageExportsResolve(
        packageJsonUrl,
        packageSubpath,
        packageConfig,
        base,
        conditions
      ).resolved
    if (packageSubpath === '.')
      return legacyMainResolve(packageJsonUrl, packageConfig, base)
    return new URL(packageSubpath, packageJsonUrl)
    // Cross-platform root check.
  } while (packageJsonPath.length !== lastPath.length)

  throw new ERR_MODULE_NOT_FOUND(packageName, fileURLToPath(base))
}

/**
 * @param {string} specifier
 * @returns {boolean}
 */
function isRelativeSpecifier(specifier) {
  if (specifier[0] === '.') {
    if (specifier.length === 1 || specifier[1] === '/') return true
    if (
      specifier[1] === '.' &&
      (specifier.length === 2 || specifier[2] === '/')
    ) {
      return true
    }
  }

  return false
}

/**
 * @param {string} specifier
 * @returns {boolean}
 */
function shouldBeTreatedAsRelativeOrAbsolutePath(specifier) {
  if (specifier === '') return false
  if (specifier[0] === '/') return true
  return isRelativeSpecifier(specifier)
}

/**
 * The “Resolver Algorithm Specification” as detailed in the Node docs (which is
 * sync and slightly lower-level than `resolve`).
 *
 *
 *
 * @param {string} specifier
 * @param {URL} base
 * @param {Set<string>} [conditions]
 * @returns {URL}
 */
export function moduleResolve(specifier, base, conditions) {
  // Order swapped from spec for minor perf gain.
  // Ok since relative URLs cannot parse as URLs.
  /** @type {URL} */
  let resolved

  if (shouldBeTreatedAsRelativeOrAbsolutePath(specifier)) {
    resolved = new URL(specifier, base)
  } else if (specifier[0] === '#') {
    ;({resolved} = packageImportsResolve(specifier, base, conditions))
  } else {
    try {
      resolved = new URL(specifier)
    } catch {
      resolved = packageResolve(specifier, base, conditions)
    }
  }

  return finalizeResolution(resolved, base)
}

/**
 * @param {string} specifier
 * @param {{parentURL?: string, conditions?: string[]}} context
 * @returns {{url: string}}
 */
export function defaultResolve(specifier, context = {}) {
  const {parentURL} = context
  /** @type {URL} */
  let parsed

  try {
    parsed = new URL(specifier)
    if (parsed.protocol === 'data:') {
      return {url: specifier}
    }
  } catch {}

  if (parsed && parsed.protocol === 'node:') return {url: specifier}
  if (parsed && parsed.protocol !== 'file:' && parsed.protocol !== 'data:')
    throw new ERR_UNSUPPORTED_ESM_URL_SCHEME(parsed)

  if (listOfBuiltins.includes(specifier)) {
    return {url: 'node:' + specifier}
  }

  if (parentURL.startsWith('data:')) {
    // This is gonna blow up, we want the error
    new URL(specifier, parentURL)
  }

  const conditions = getConditionsSet(context.conditions)
  let url = moduleResolve(specifier, new URL(parentURL), conditions)

  const urlPath = fileURLToPath(url)
  const real = realpathSync(urlPath)
  const old = url
  url = pathToFileURL(real + (urlPath.endsWith(path.sep) ? '/' : ''))
  url.search = old.search
  url.hash = old.hash

  return {url: `${url}`}
}
