// Base class for npm commands

const { relative } = require('path')

const usageUtil = require('./utils/usage.js')
const ConfigDefinitions = require('./utils/config/definitions.js')
const getWorkspaces = require('./workspaces/get-workspaces.js')

class BaseCommand {
  constructor (npm) {
    this.wrapWidth = 80
    this.npm = npm
  }

  get name () {
    return this.constructor.name
  }

  get description () {
    return this.constructor.description
  }

  get ignoreImplicitWorkspace () {
    return this.constructor.ignoreImplicitWorkspace
  }

  get usage () {
    let usage = `npm ${this.constructor.name}\n\n`
    if (this.constructor.description) {
      usage = `${usage}${this.constructor.description}\n\n`
    }

    usage = `${usage}Usage:\n`
    if (!this.constructor.usage) {
      usage = `${usage}npm ${this.constructor.name}`
    } else {
      usage = `${usage}${this.constructor.usage
        .map(u => `npm ${this.constructor.name} ${u}`)
        .join('\n')}`
    }

    if (this.constructor.params) {
      usage = `${usage}\n\nOptions:\n${this.wrappedParams}`
    }

    // Mostly this just appends aliases, this could be more clear
    usage = usageUtil(this.constructor.name, usage)
    usage = `${usage}\n\nRun "npm help ${this.constructor.name}" for more info`
    return usage
  }

  get wrappedParams () {
    let results = ''
    let line = ''

    for (const param of this.constructor.params) {
      const usage = `[${ConfigDefinitions[param].usage}]`
      if (line.length && line.length + usage.length > this.wrapWidth) {
        results = [results, line].filter(Boolean).join('\n')
        line = ''
      }
      line = [line, usage].filter(Boolean).join(' ')
    }
    results = [results, line].filter(Boolean).join('\n')
    return results
  }

  usageError (prefix = '') {
    if (prefix) {
      prefix += '\n\n'
    }
    return Object.assign(new Error(`\nUsage: ${prefix}${this.usage}`), {
      code: 'EUSAGE',
    })
  }

  async execWorkspaces (args, filters) {
    throw Object.assign(new Error('This command does not support workspaces.'), {
      code: 'ENOWORKSPACES',
    })
  }

  async setWorkspaces (filters) {
    if (this.isArboristCmd) {
      this.includeWorkspaceRoot = false
    }

    const relativeFrom = relative(this.npm.localPrefix, process.cwd()).startsWith('..')
      ? this.npm.localPrefix
      : process.cwd()

    const ws = await getWorkspaces(filters, {
      path: this.npm.localPrefix,
      includeWorkspaceRoot: this.includeWorkspaceRoot,
      relativeFrom,
    })
    this.workspaces = ws
    this.workspaceNames = [...ws.keys()]
    this.workspacePaths = [...ws.values()]
  }
}
module.exports = BaseCommand
