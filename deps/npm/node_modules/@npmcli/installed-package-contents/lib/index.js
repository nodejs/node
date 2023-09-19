#! /usr/bin/env node

// to GET CONTENTS for folder at PATH (which may be a PACKAGE):
// - if PACKAGE, read path/package.json
//   - if bins in ../node_modules/.bin, add those to result
// - if depth >= maxDepth, add PATH to result, and finish
// - readdir(PATH, with file types)
// - add all FILEs in PATH to result
// - if PARENT:
//   - if depth < maxDepth, add GET CONTENTS of all DIRs in PATH
//   - else, add all DIRs in PATH
// - if no parent
//   - if no bundled deps,
//     - if depth < maxDepth, add GET CONTENTS of DIRs in path except
//       node_modules
//     - else, add all DIRs in path other than node_modules
//   - if has bundled deps,
//     - get list of bundled deps
//     - add GET CONTENTS of bundled deps, PACKAGE=true, depth + 1

const bundled = require('npm-bundled')
const { promisify } = require('util')
const fs = require('fs')
const readFile = promisify(fs.readFile)
const readdir = promisify(fs.readdir)
const stat = promisify(fs.stat)
const lstat = promisify(fs.lstat)
const { relative, resolve, basename, dirname } = require('path')
const normalizePackageBin = require('npm-normalize-package-bin')

const readPackage = ({ path, packageJsonCache }) =>
  packageJsonCache.has(path) ? Promise.resolve(packageJsonCache.get(path))
  : readFile(path).then(json => {
    const pkg = normalizePackageBin(JSON.parse(json))
    packageJsonCache.set(path, pkg)
    return pkg
  })
    .catch(er => null)

// just normalize bundle deps and bin, that's all we care about here.
const normalized = Symbol('package data has been normalized')
const rpj = ({ path, packageJsonCache }) =>
  readPackage({ path, packageJsonCache })
    .then(pkg => {
      if (!pkg || pkg[normalized]) {
        return pkg
      }
      if (pkg.bundledDependencies && !pkg.bundleDependencies) {
        pkg.bundleDependencies = pkg.bundledDependencies
        delete pkg.bundledDependencies
      }
      const bd = pkg.bundleDependencies
      if (bd === true) {
        pkg.bundleDependencies = [
          ...Object.keys(pkg.dependencies || {}),
          ...Object.keys(pkg.optionalDependencies || {}),
        ]
      }
      if (typeof bd === 'object' && !Array.isArray(bd)) {
        pkg.bundleDependencies = Object.keys(bd)
      }
      pkg[normalized] = true
      return pkg
    })

const pkgContents = async ({
  path,
  depth,
  currentDepth = 0,
  pkg = null,
  result = null,
  packageJsonCache = null,
}) => {
  if (!result) {
    result = new Set()
  }

  if (!packageJsonCache) {
    packageJsonCache = new Map()
  }

  if (pkg === true) {
    return rpj({ path: path + '/package.json', packageJsonCache })
      .then(p => pkgContents({
        path,
        depth,
        currentDepth,
        pkg: p,
        result,
        packageJsonCache,
      }))
  }

  if (pkg) {
    // add all bins to result if they exist
    if (pkg.bin) {
      const dir = dirname(path)
      const scope = basename(dir)
      const nm = /^@.+/.test(scope) ? dirname(dir) : dir

      const binFiles = []
      Object.keys(pkg.bin).forEach(b => {
        const base = resolve(nm, '.bin', b)
        binFiles.push(base, base + '.cmd', base + '.ps1')
      })

      const bins = await Promise.all(
        binFiles.map(b => stat(b).then(() => b).catch((er) => null))
      )
      bins.filter(b => b).forEach(b => result.add(b))
    }
  }

  if (currentDepth >= depth) {
    result.add(path)
    return result
  }

  // we'll need bundle list later, so get that now in parallel
  const [dirEntries, bundleDeps] = await Promise.all([
    readdir(path, { withFileTypes: true }),
    currentDepth === 0 && pkg && pkg.bundleDependencies
      ? bundled({ path, packageJsonCache }) : null,
  ]).catch(() => [])

  // not a thing, probably a missing folder
  if (!dirEntries) {
    return result
  }

  // empty folder, just add the folder itself to the result
  if (!dirEntries.length && !bundleDeps && currentDepth !== 0) {
    result.add(path)
    return result
  }

  const recursePromises = []

  // if we didn't get withFileTypes support, tack that on
  if (typeof dirEntries[0] === 'string') {
    // use a map so we can return a promise, but we mutate dirEntries in place
    // this is much slower than getting the entries from the readdir call,
    // but polyfills support for node versions before 10.10
    await Promise.all(dirEntries.map(async (name, index) => {
      const p = resolve(path, name)
      const st = await lstat(p)
      dirEntries[index] = Object.assign(st, { name })
    }))
  }

  for (const entry of dirEntries) {
    const p = resolve(path, entry.name)
    if (entry.isDirectory() === false) {
      result.add(p)
      continue
    }

    if (currentDepth !== 0 || entry.name !== 'node_modules') {
      if (currentDepth < depth - 1) {
        recursePromises.push(pkgContents({
          path: p,
          packageJsonCache,
          depth,
          currentDepth: currentDepth + 1,
          result,
        }))
      } else {
        result.add(p)
      }
      continue
    }
  }

  if (bundleDeps) {
    // bundle deps are all folders
    // we always recurse to get pkg bins, but if currentDepth is too high,
    // it'll return early before walking their contents.
    recursePromises.push(...bundleDeps.map(dep => {
      const p = resolve(path, 'node_modules', dep)
      return pkgContents({
        path: p,
        packageJsonCache,
        pkg: true,
        depth,
        currentDepth: currentDepth + 1,
        result,
      })
    }))
  }

  if (recursePromises.length) {
    await Promise.all(recursePromises)
  }

  return result
}

module.exports = ({ path, depth = 1, packageJsonCache }) => pkgContents({
  path: resolve(path),
  depth,
  pkg: true,
  packageJsonCache,
}).then(results => [...results])

if (require.main === module) {
  const options = { path: null, depth: 1 }
  const usage = `Usage:
  installed-package-contents <path> [-d<n> --depth=<n>]

Lists the files installed for a package specified by <path>.

Options:
  -d<n> --depth=<n>   Provide a numeric value ("Infinity" is allowed)
                      to specify how deep in the file tree to traverse.
                      Default=1
  -h --help           Show this usage information`

  process.argv.slice(2).forEach(arg => {
    let match
    if ((match = arg.match(/^--depth=([0-9]+|Infinity)/)) ||
        (match = arg.match(/^-d([0-9]+|Infinity)/))) {
      options.depth = +match[1]
    } else if (arg === '-h' || arg === '--help') {
      console.log(usage)
      process.exit(0)
    } else {
      options.path = arg
    }
  })
  if (!options.path) {
    console.error('ERROR: no path provided')
    console.error(usage)
    process.exit(1)
  }
  const cwd = process.cwd()
  module.exports(options)
    .then(list => list.sort().forEach(p => console.log(relative(cwd, p))))
    .catch(/* istanbul ignore next - pretty unusual */ er => {
      console.error(er)
      process.exit(1)
    })
}
