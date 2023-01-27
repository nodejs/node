// Base class for npm commands

const { relative } = require('path')

const ConfigDefinitions = require('./utils/config/definitions.js')
const getWorkspaces = require('./workspaces/get-workspaces.js')

const cmdAliases = require('./utils/cmd-list').aliases

class BaseCommand {
  static workspaces = false
  static ignoreImplicitWorkspace = true

  constructor (npm) {
    this.wrapWidth = 80
    this.npm = npm

    const { config } = this.npm

    if (!this.constructor.skipConfigValidation) {
      config.validate()
    }

    if (config.get('workspaces') === false && config.get('workspace').length) {
      throw new Error('Can not use --no-workspaces and --workspace at the same time')
    }
  }

  get name () {
    return this.constructor.name
  }

  get description () {
    return this.constructor.description
  }

  get params () {
    return this.constructor.params
  }

  get usage () {
    const usage = [
      `${this.description}`,
      '',
      'Usage:',
    ]

    if (!this.constructor.usage) {
      usage.push(`npm ${this.name}`)
    } else {
      usage.push(...this.constructor.usage.map(u => `npm ${this.name} ${u}`))
    }

    if (this.params) {
      usage.push('')
      usage.push('Options:')
      usage.push(this.wrappedParams)
    }

    const aliases = Object.keys(cmdAliases).reduce((p, c) => {
      if (cmdAliases[c] === this.name) {
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
    usage.push(`Run "npm help ${this.name}" for more info`)

    return usage.join('\n')
  }

  get wrappedParams () {
    let results = ''
    let line = ''

    for (const param of this.params) {
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

  async cmdExec (args) {
    const { config } = this.npm

    if (config.get('usage')) {
      return this.npm.output(this.usage)
    }

    const hasWsConfig = config.get('workspaces') || config.get('workspace').length
    // if cwd is a workspace, the default is set to [that workspace]
    const implicitWs = config.get('workspace', 'default').length

    // (-ws || -w foo) && (cwd is not a workspace || command is not ignoring implicit workspaces)
    if (hasWsConfig && (!implicitWs || !this.constructor.ignoreImplicitWorkspace)) {
      if (this.npm.global) {
        throw new Error('Workspaces not supported for global packages')
      }
      if (!this.constructor.workspaces) {
        throw Object.assign(new Error('This command does not support workspaces.'), {
          code: 'ENOWORKSPACES',
        })
      }
      return this.execWorkspaces(args)
    }

    return this.exec(args)
  }

  async setWorkspaces () {
    const includeWorkspaceRoot = this.isArboristCmd
      ? false
      : this.npm.config.get('include-workspace-root')

    const prefixInsideCwd = relative(this.npm.localPrefix, process.cwd()).startsWith('..')
    const relativeFrom = prefixInsideCwd ? this.npm.localPrefix : process.cwd()

    const filters = this.npm.config.get('workspace')
    const ws = await getWorkspaces(filters, {
      path: this.npm.localPrefix,
      includeWorkspaceRoot,
      relativeFrom,
    })

    this.workspaces = ws
    this.workspaceNames = [...ws.keys()]
    this.workspacePaths = [...ws.values()]
  }
}
module.exports = BaseCommand
