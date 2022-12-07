// Base class for npm commands

const { relative } = require('path')

const ConfigDefinitions = require('./utils/config/definitions.js')
const getWorkspaces = require('./workspaces/get-workspaces.js')

const cmdAliases = require('./utils/cmd-list').aliases

class BaseCommand {
  constructor (npm) {
    this.wrapWidth = 80
    this.npm = npm

    if (!this.skipConfigValidation) {
      this.npm.config.validate()
    }
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

  get skipConfigValidation () {
    return this.constructor.skipConfigValidation
  }

  get usage () {
    const usage = [
      `${this.constructor.description}`,
      '',
      'Usage:',
    ]

    if (!this.constructor.usage) {
      usage.push(`npm ${this.constructor.name}`)
    } else {
      usage.push(...this.constructor.usage.map(u => `npm ${this.constructor.name} ${u}`))
    }

    if (this.constructor.params) {
      usage.push('')
      usage.push('Options:')
      usage.push(this.wrappedParams)
    }

    const aliases = Object.keys(cmdAliases).reduce((p, c) => {
      if (cmdAliases[c] === this.constructor.name) {
        p.push(c)
      }
      return p
    }, [])

    if (aliases.length === 1) {
      usage.push('')
      usage.push(`alias: ${aliases.join(', ')}`)
    } else if (aliases.length > 1) {
      usage.push('')
      usage.push(`aliases: ${aliases.join(', ')}`)
    }

    usage.push('')
    usage.push(`Run "npm help ${this.constructor.name}" for more info`)

    return usage.join('\n')
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
    return Object.assign(new Error(`\n${prefix}${this.usage}`), {
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
