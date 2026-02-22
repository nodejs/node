const { log } = require('proc-log')
const { definitions, shorthands } = require('@npmcli/config/lib/definitions')
const nopt = require('nopt')

class BaseCommand {
  // these defaults can be overridden by individual commands
  static workspaces = false
  static ignoreImplicitWorkspace = true
  static checkDevEngines = false

  // these should always be overridden by individual commands
  static name = null
  static description = null
  static params = null
  static definitions = null
  static subcommands = null
  // Number of expected positional arguments (null = unlimited/unchecked)
  static positionals = null

  // this is a static so that we can read from it without instantiating a command
  // which would require loading the config
  static get describeUsage () {
    return this.getUsage()
  }

  static getUsage (parentName = null, includeDescriptions = true) {
    const { aliases: cmdAliases } = require('./utils/cmd-list')
    const seenExclusive = new Set()
    const wrapWidth = 80
    const { description, usage = [''], name } = this

    // Resolve to a definitions array: if the command has its own definitions, use
    // those directly; otherwise resolve params from the global definitions pool.
    let cmdDefs
    if (this.definitions) {
      cmdDefs = this.definitions
    } else if (this.params) {
      cmdDefs = this.params.map(p => definitions[p]).filter(Boolean)
    }

    // If this is a subcommand, prepend parent name
    const fullCommandName = parentName ? `${parentName} ${name}` : name

    const fullUsage = [
      `${description}`,
      '',
      'Usage:',
      ...usage.map(u => `npm ${fullCommandName} ${u}`.trim()),
    ]

    if (this.subcommands) {
      fullUsage.push('')
      fullUsage.push('Subcommands:')
      const subcommandEntries = Object.entries(this.subcommands)
      for (let i = 0; i < subcommandEntries.length; i++) {
        const [subName, SubCommand] = subcommandEntries[i]
        fullUsage.push(`  ${subName}`)
        if (SubCommand.description) {
          fullUsage.push(`    ${SubCommand.description}`)
        }
        // Add space between subcommands except after the last one
        if (i < subcommandEntries.length - 1) {
          fullUsage.push('')
        }
      }
      fullUsage.push('')
      fullUsage.push(`Run "npm ${name} <subcommand> --help" for more info on a subcommand.`)
    }

    if (cmdDefs) {
      let results = ''
      let line = ''
      for (const def of cmdDefs) {
        /* istanbul ignore next */
        if (seenExclusive.has(def.key)) {
          continue
        }
        let paramUsage = def.usage
        if (def.exclusive) {
          const exclusiveParams = [paramUsage]
          for (const e of def.exclusive) {
            seenExclusive.add(e)
            const eDef = cmdDefs.find(d => d.key === e) || definitions[e]
            exclusiveParams.push(eDef?.usage)
          }
          paramUsage = `${exclusiveParams.join('|')}`
        }
        paramUsage = `[${paramUsage}]`
        if (line.length + paramUsage.length > wrapWidth) {
          results = [results, line].filter(Boolean).join('\n')
          line = ''
        }
        line = [line, paramUsage].filter(Boolean).join(' ')
      }
      fullUsage.push('')
      fullUsage.push('Options:')
      fullUsage.push([results, line].filter(Boolean).join('\n'))

      // Add flag descriptions
      if (cmdDefs.length > 0 && includeDescriptions) {
        fullUsage.push('')
        for (const def of cmdDefs) {
          if (def.description) {
            const desc = def.description.trim().split('\n')[0]
            const shortcuts = def.short ? `-${def.short}` : ''
            const aliases = (def.alias || []).map(v => `--${v}`).join('|')
            const mainFlag = `--${def.key}`
            const flagName = [shortcuts, mainFlag, aliases].filter(Boolean).join('|')
            const requiredNote = def.required ? ' (required)' : ''
            fullUsage.push(`  ${flagName}${requiredNote}`)
            fullUsage.push(`    ${desc}`)
            fullUsage.push('')
          }
        }
      }
    }

    const aliases = Object.entries(cmdAliases).reduce((p, [k, v]) => {
      return p.concat(v === name ? k : [])
    }, [])

    if (aliases.length) {
      const plural = aliases.length === 1 ? '' : 'es'
      fullUsage.push('')
      fullUsage.push(`alias${plural}: ${aliases.join(', ')}`)
    }

    fullUsage.push('')
    fullUsage.push(`Run "npm help ${name}" for more info`)

    return fullUsage.join('\n')
  }

  constructor (npm) {
    this.npm = npm
    this.commandArgs = null

    const { config } = this

    if (!this.constructor.skipConfigValidation) {
      config.validate()
    }

    if (config.get('workspaces') === false && config.get('workspace').length) {
      throw new Error('Cannot use --no-workspaces and --workspace at the same time')
    }
  }

  get config () {
    // Return command-specific config if it exists, otherwise use npm's config
    return this.npm.config
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
    return this.constructor.describeUsage
  }

  usageError (prefix = '') {
    if (prefix) {
      prefix += '\n\n'
    }
    return Object.assign(new Error(`\n${prefix}${this.usage}`), {
      code: 'EUSAGE',
    })
  }

  // Compare the number of entries with what was expected
  checkExpected (entries) {
    if (!this.npm.config.isDefault('expect-results')) {
      const expected = this.npm.config.get('expect-results')
      if (!!entries !== !!expected) {
        log.warn(this.name, `Expected ${expected ? '' : 'no '}results, got ${entries}`)
        process.exitCode = 1
      }
    } else if (!this.npm.config.isDefault('expect-result-count')) {
      const expected = this.npm.config.get('expect-result-count')
      if (expected !== entries) {
        log.warn(this.name, `Expected ${expected} result${expected === 1 ? '' : 's'}, got ${entries}`)
        process.exitCode = 1
      }
    }
  }

  // Checks the devEngines entry in the package.json at this.localPrefix
  async checkDevEngines () {
    const force = this.npm.flatOptions.force

    const { devEngines } = await require('@npmcli/package-json')
      .normalize(this.npm.config.localPrefix)
      .then(p => p.content)
      .catch(() => ({}))

    if (typeof devEngines === 'undefined') {
      return
    }

    const { checkDevEngines, currentEnv } = require('npm-install-checks')
    const current = currentEnv.devEngines({
      nodeVersion: this.npm.nodeVersion,
      npmVersion: this.npm.version,
    })

    const failures = checkDevEngines(devEngines, current)
    const warnings = failures.filter(f => f.isWarn)
    const errors = failures.filter(f => f.isError)

    const genMsg = (failure, i = 0) => {
      return [...new Set([
        // eslint-disable-next-line
        i === 0 ? 'The developer of this package has specified the following through devEngines' : '',
        `${failure.message}`,
        `${failure.errors.map(e => e.message).join('\n')}`,
      ])].filter(v => v).join('\n')
    }

    [...warnings, ...(force ? errors : [])].forEach((failure, i) => {
      const message = genMsg(failure, i)
      log.warn('EBADDEVENGINES', message)
      log.warn('EBADDEVENGINES', {
        current: failure.current,
        required: failure.required,
      })
    })

    if (force) {
      return
    }

    if (errors.length) {
      const failure = errors[0]
      const message = genMsg(failure)
      throw Object.assign(new Error(message), {
        engine: failure.engine,
        code: 'EBADDEVENGINES',
        current: failure.current,
        required: failure.required,
      })
    }
  }

  async setWorkspaces () {
    const { relative } = require('node:path')

    const includeWorkspaceRoot = this.isArboristCmd
      ? false
      : this.npm.config.get('include-workspace-root')

    const prefixInsideCwd = relative(this.npm.localPrefix, process.cwd()).startsWith('..')
    const relativeFrom = prefixInsideCwd ? this.npm.localPrefix : process.cwd()

    const filters = this.npm.config.get('workspace')
    const getWorkspaces = require('./utils/get-workspaces.js')
    const ws = await getWorkspaces(filters, {
      path: this.npm.localPrefix,
      includeWorkspaceRoot,
      relativeFrom,
    })

    this.workspaces = ws
    this.workspaceNames = [...ws.keys()]
    this.workspacePaths = [...ws.values()]
  }

  flags (depth = 1) {
    const commandDefinitions = this.constructor.definitions || []

    // Build types, shorthands, and defaults from definitions
    const types = {}
    const defaults = {}
    const cmdShorthands = {}
    const aliasMap = {} // Track which aliases map to which main keys

    for (const def of commandDefinitions) {
      defaults[def.key] = def.default
      types[def.key] = def.type

      // Handle aliases defined in the definition
      if (def.alias && Array.isArray(def.alias)) {
        for (const aliasKey of def.alias) {
          types[aliasKey] = def.type // Needed for nopt to parse aliases
          if (!aliasMap[def.key]) {
            aliasMap[def.key] = []
          }
          aliasMap[def.key].push(aliasKey)
        }
      }

      // Handle short options
      if (def.short) {
        const shorts = Array.isArray(def.short) ? def.short : [def.short]
        for (const short of shorts) {
          cmdShorthands[short] = [`--${def.key}`]
        }
      }
    }

    // Parse args
    let parsed = {}
    let remains = []
    const argv = this.config.argv
    if (argv && argv.length > 0) {
      // config.argv contains the full command line including node, npm, and command names
      // Format: ['node', 'npm', 'command', 'subcommand', 'positional', '--flags']
      // depth tells us how many command names to skip (1 for top-level, 2 for subcommand, etc.)
      const offset = 2 + depth // Skip 'node', 'npm', and all command/subcommand names
      parsed = nopt(types, cmdShorthands, argv, offset)
      remains = parsed.argv.remain
      delete parsed.argv
    }

    // Validate flags - only if command has definitions (new system)
    if (this.constructor.definitions && this.constructor.definitions.length > 0) {
      this.#validateFlags(parsed, commandDefinitions, remains)
    }

    // Check for conflicts between main flags and their aliases
    // Also map aliases back to their main keys
    for (const [mainKey, aliases] of Object.entries(aliasMap)) {
      const providedKeys = []
      if (mainKey in parsed) {
        providedKeys.push(mainKey)
      }
      for (const alias of aliases) {
        if (alias in parsed) {
          providedKeys.push(alias)
        }
      }
      if (providedKeys.length > 1) {
        const flagList = providedKeys.map(k => `--${k}`).join(' or ')
        throw new Error(`Please provide only one of ${flagList}`)
      }

      // If an alias was provided, map it to the main key
      if (providedKeys.length === 1 && providedKeys[0] !== mainKey) {
        const aliasKey = providedKeys[0]
        parsed[mainKey] = parsed[aliasKey]
        delete parsed[aliasKey]
      }
    }

    // Only include keys that are defined in commandDefinitions (main keys only)
    const filtered = {}
    for (const def of commandDefinitions) {
      if (def.key in parsed) {
        filtered[def.key] = parsed[def.key]
      }
    }
    return [{ ...defaults, ...filtered }, remains]
  }

  // Validate flags and throw errors for unknown flags or unexpected positionals
  #validateFlags (parsed, commandDefinitions, remains) {
    // Build a set of all valid flag names (global + command-specific + shorthands)
    const validFlags = new Set([
      ...Object.keys(definitions),
      ...commandDefinitions.map(d => d.key),
      ...Object.keys(shorthands), // Add global shorthands like 'verbose', 'dd', etc.
    ])

    // Add aliases to valid flags
    for (const def of commandDefinitions) {
      if (def.alias && Array.isArray(def.alias)) {
        for (const alias of def.alias) {
          validFlags.add(alias)
        }
      }
    }

    // Check parsed flags against valid flags
    const unknownFlags = []
    for (const key of Object.keys(parsed)) {
      if (!validFlags.has(key)) {
        unknownFlags.push(key)
      }
    }

    // Throw error if unknown flags were found
    if (unknownFlags.length > 0) {
      const flagList = unknownFlags.map(f => `--${f}`).join(', ')
      throw this.usageError(`Unknown flag${unknownFlags.length > 1 ? 's' : ''}: ${flagList}`)
    }

    // Remove warnings for command-specific definitions that npm's global config
    // doesn't know about (these were queued as "unknown" during config.load())
    for (const def of commandDefinitions) {
      this.npm.config.removeWarning(def.key)
      if (def.alias && Array.isArray(def.alias)) {
        for (const alias of def.alias) {
          this.npm.config.removeWarning(alias)
        }
      }
    }

    // Remove warnings for unknown positionals that were actually consumed as flag values
    // by command-specific definitions (e.g., --id <value> where --id is command-specific)
    const remainsSet = new Set(remains)
    for (const unknownPos of this.npm.config.getUnknownPositionals()) {
      if (!remainsSet.has(unknownPos)) {
        // This value was consumed as a flag value, not truly a positional
        this.npm.config.removeUnknownPositional(unknownPos)
      }
    }

    // Warn about extra positional arguments beyond what the command expects
    const expectedPositionals = this.constructor.positionals
    if (expectedPositionals !== null && remains.length > expectedPositionals) {
      const extraPositionals = remains.slice(expectedPositionals)
      for (const extra of extraPositionals) {
        throw new Error(`Unknown positional argument: ${extra}`)
      }
    }

    this.npm.config.logWarnings()
  }

  async exec () {
    // This method should be overridden by commands
    // Subcommand routing is handled in npm.js #exec
  }
}

module.exports = BaseCommand
