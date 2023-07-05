const { readFile, writeFile } = require('fs/promises')
const { resolve } = require('path')
const updateDeps = require('./update-dependencies.js')
const updateScripts = require('./update-scripts.js')
const updateWorkspaces = require('./update-workspaces.js')
const normalize = require('./normalize.js')

const parseJSON = require('json-parse-even-better-errors')

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
  static normalizeSteps = Object.freeze([
    '_id',
    '_attributes',
    'bundledDependencies',
    'bundleDependencies',
    'optionalDedupe',
    'scripts',
    'funding',
    'bin',
  ])

  static prepareSteps = Object.freeze([
    '_attributes',
    'bundledDependencies',
    'bundleDependencies',
    'bundleDependenciesDeleteFalse',
    'gypfile',
    'serverjs',
    'scriptpath',
    'authors',
    'readme',
    'mans',
    'binDir',
    'gitHead',
    'fillTypes',
    'normalizeData',
    'binRefs',
  ])

  // default behavior, just loads and parses
  static async load (path) {
    return await new PackageJson(path).load()
  }

  // read-package-json compatible behavior
  static async prepare (path, opts) {
    return await new PackageJson(path).prepare(opts)
  }

  // read-package-json-fast compatible behavior
  static async normalize (path, opts) {
    return await new PackageJson(path).normalize(opts)
  }

  #filename
  #path
  #manifest = {}
  #readFileContent = ''
  #fromIndex = false

  constructor (path) {
    this.#path = path
    this.#filename = resolve(path, 'package.json')
  }

  async load (parseIndex) {
    let parseErr
    try {
      this.#readFileContent =
        await readFile(this.#filename, 'utf8')
    } catch (err) {
      err.message = `Could not read package.json: ${err}`
      if (!parseIndex) {
        throw err
      }
      parseErr = err
    }

    if (parseErr) {
      const indexFile = resolve(this.#path, 'index.js')
      let indexFileContent
      try {
        indexFileContent = await readFile(indexFile, 'utf8')
      } catch (err) {
        throw parseErr
      }
      try {
        this.#manifest = fromComment(indexFileContent)
      } catch (err) {
        throw parseErr
      }
      this.#fromIndex = true
      return this
    }

    try {
      this.#manifest = parseJSON(this.#readFileContent)
    } catch (err) {
      err.message = `Invalid package.json: ${err}`
      throw err
    }
    return this
  }

  get content () {
    return this.#manifest
  }

  get path () {
    return this.#path
  }

  update (content) {
    // validates both current manifest and content param
    const invalidContent =
      typeof this.#manifest !== 'object'
        || typeof content !== 'object'
    if (invalidContent) {
      throw Object.assign(
        new Error(`Can't update invalid package.json data`),
        { code: 'EPACKAGEJSONUPDATE' }
      )
    }

    for (const step of knownSteps) {
      this.#manifest = step({ content, originalContent: this.#manifest })
    }

    // unknown properties will just be overwitten
    for (const [key, value] of Object.entries(content)) {
      if (!knownKeys.has(key)) {
        this.#manifest[key] = value
      }
    }

    return this
  }

  async save () {
    if (this.#fromIndex) {
      throw new Error('No package.json to save to')
    }
    const {
      [Symbol.for('indent')]: indent,
      [Symbol.for('newline')]: newline,
    } = this.#manifest

    const format = indent === undefined ? '  ' : indent
    const eol = newline === undefined ? '\n' : newline
    const fileContent = `${
      JSON.stringify(this.#manifest, null, format)
    }\n`
      .replace(/\n/g, eol)

    if (fileContent.trim() !== this.#readFileContent.trim()) {
      return await writeFile(this.#filename, fileContent)
    }
  }

  async normalize (opts = {}) {
    if (!opts.steps) {
      opts.steps = this.constructor.normalizeSteps
    }
    await this.load()
    await normalize(this, opts)
    return this
  }

  async prepare (opts = {}) {
    if (!opts.steps) {
      opts.steps = this.constructor.prepareSteps
    }
    await this.load(true)
    await normalize(this, opts)
    return this
  }
}

// /**package { "name": "foo", "version": "1.2.3", ... } **/
function fromComment (data) {
  data = data.split(/^\/\*\*package(?:\s|$)/m)

  if (data.length < 2) {
    throw new Error('File has no package in comments')
  }
  data = data[1]
  data = data.split(/\*\*\/$/m)

  if (data.length < 2) {
    throw new Error('File has no package in comments')
  }
  data = data[0]
  data = data.replace(/^\s*\*/mg, '')

  return parseJSON(data)
}

module.exports = PackageJson
