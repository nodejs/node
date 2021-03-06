const fs = require('fs')
const promisify = require('util').promisify
const readFile = promisify(fs.readFile)
const writeFile = promisify(fs.writeFile)
const {resolve} = require('path')

const parseJSON = require('json-parse-even-better-errors')

const { orderDeps } = require('./dep-spec.js')

const depTypes = new Set([
  'dependencies',
  'optionalDependencies',
  'devDependencies',
  'peerDependencies',
])

const parseJsonSafe = json => {
  try {
    return parseJSON(json)
  } catch (er) {
    return null
  }
}

const updateRootPackageJson = async tree => {
  const filename = resolve(tree.path, 'package.json')
  const originalJson = await readFile(filename, 'utf8').catch(() => null)
  const originalContent = parseJsonSafe(originalJson)

  const depsData = orderDeps({
    ...tree.package,
  })

  // optionalDependencies don't need to be repeated in two places
  if (depsData.dependencies) {
    if (depsData.optionalDependencies) {
      for (const name of Object.keys(depsData.optionalDependencies))
        delete depsData.dependencies[name]
    }
    if (Object.keys(depsData.dependencies).length === 0)
      delete depsData.dependencies
  }

  // if there's no package.json, just use internal pkg info as source of truth
  // clone the object though, so we can still refer to what it originally was
  const packageJsonContent = !originalContent ? depsData
    : Object.assign({}, originalContent)

  // loop through all types of dependencies and update package json content
  for (const type of depTypes)
    packageJsonContent[type] = depsData[type]

  // if original package.json had dep in peerDeps AND deps, preserve that.
  const { dependencies: origProd, peerDependencies: origPeer } =
    originalContent || {}
  const { peerDependencies: newPeer } = packageJsonContent
  if (origProd && origPeer && newPeer) {
    // we have original prod/peer deps, and new peer deps
    // copy over any that were in both in the original
    for (const name of Object.keys(origPeer)) {
      if (origProd[name] !== undefined && newPeer[name] !== undefined) {
        packageJsonContent.dependencies = packageJsonContent.dependencies || {}
        packageJsonContent.dependencies[name] = newPeer[name]
      }
    }
  }

  // format content
  const {
    [Symbol.for('indent')]: indent,
    [Symbol.for('newline')]: newline,
  } = tree.package
  const format = indent === undefined ? '  ' : indent
  const eol = newline === undefined ? '\n' : newline
  const content = (JSON.stringify(packageJsonContent, null, format) + '\n')
    .replace(/\n/g, eol)

  if (content !== originalJson)
    return writeFile(filename, content)
}

module.exports = updateRootPackageJson
