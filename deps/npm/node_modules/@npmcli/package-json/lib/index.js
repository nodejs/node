const fs = require('fs')
const promisify = require('util').promisify
const readFile = promisify(fs.readFile)
const writeFile = promisify(fs.writeFile)
const { resolve } = require('path')
const updateDeps = require('./update-dependencies.js')
const updateScripts = require('./update-scripts.js')
const updateWorkspaces = require('./update-workspaces.js')

const parseJSON = require('json-parse-even-better-errors')

const _filename = Symbol('filename')
const _manifest = Symbol('manifest')
const _readFileContent = Symbol('readFileContent')

// a list of handy specialized helper functions that take
// care of special cases that are handled by the npm cli
const knownSteps = new Set([
  updateDeps,
  updateScripts,
  updateWorkspaces,
])

// list of all keys that are handled by "knownSteps" helpers
const knownKeys = new Set([
  ...updateDeps.knownKeys,
  'scripts',
  'workspaces',
])

class PackageJson {
  static async load (path) {
    return await new PackageJson(path).load()
  }

  constructor (path) {
    this[_filename] = resolve(path, 'package.json')
    this[_manifest] = {}
    this[_readFileContent] = ''
  }

  async load () {
    try {
      this[_readFileContent] =
        await readFile(this[_filename], 'utf8')
    } catch (err) {
      throw new Error('package.json not found')
    }

    try {
      this[_manifest] =
        parseJSON(this[_readFileContent])
    } catch (err) {
      throw new Error(`Invalid package.json: ${err}`)
    }

    return this
  }

  get content () {
    return this[_manifest]
  }

  update (content) {
    // validates both current manifest and content param
    const invalidContent =
      typeof this[_manifest] !== 'object'
        || typeof content !== 'object'
    if (invalidContent) {
      throw Object.assign(
        new Error(`Can't update invalid package.json data`),
        { code: 'EPACKAGEJSONUPDATE' }
      )
    }

    for (const step of knownSteps) {
      this[_manifest] = step({ content, originalContent: this[_manifest] })
    }

    // unknown properties will just be overwitten
    for (const [key, value] of Object.entries(content)) {
      if (!knownKeys.has(key)) {
        this[_manifest][key] = value
      }
    }

    return this
  }

  async save () {
    const {
      [Symbol.for('indent')]: indent,
      [Symbol.for('newline')]: newline,
    } = this[_manifest]

    const format = indent === undefined ? '  ' : indent
    const eol = newline === undefined ? '\n' : newline
    const fileContent = `${
      JSON.stringify(this[_manifest], null, format)
    }\n`
      .replace(/\n/g, eol)

    if (fileContent.trim() !== this[_readFileContent].trim()) {
      return await writeFile(this[_filename], fileContent)
    }
  }
}

module.exports = PackageJson
