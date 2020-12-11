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

async function updateRootPackageJson ({ tree }) {
  const filename = resolve(tree.path, 'package.json')
  const originalContent = await readFile(filename, 'utf8')
    .then(data => parseJSON(data))
    .catch(() => null)

  const depsData = orderDeps({
    ...tree.package,
  })

  // if there's no package.json, just use internal pkg info as source of truth
  const packageJsonContent = originalContent || depsData

  // loop through all types of dependencies and update package json content
  for (const type of depTypes)
    packageJsonContent[type] = depsData[type]

  // format content
  const {
    [Symbol.for('indent')]: indent,
    [Symbol.for('newline')]: newline,
  } = tree.package
  const format = indent === undefined ? '  ' : indent
  const eol = newline === undefined ? '\n' : newline
  const content = (JSON.stringify(packageJsonContent, null, format) + '\n')
    .replace(/\n/g, eol)

  return writeFile(filename, content)
}

module.exports = updateRootPackageJson
