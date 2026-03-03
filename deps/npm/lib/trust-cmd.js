const BaseCommand = require('./base-cmd.js')
const { otplease } = require('./utils/auth.js')
const npmFetch = require('npm-registry-fetch')
const npa = require('npm-package-arg')
const { read: _read } = require('read')
const { input, output, log, META } = require('proc-log')
const gitinfo = require('hosted-git-info')
const pkgJson = require('@npmcli/package-json')

const NPM_FRONTEND = 'https://www.npmjs.com'

class TrustCommand extends BaseCommand {
  // Helper to format template strings with color
  // Blue text with reset color for interpolated values
  warnString (strings, ...values) {
    const chalk = this.npm.chalk
    const message = strings.reduce((result, str, i) => {
      return result + chalk.blue(str) + (values[i] ? chalk.reset(values[i]) : '')
    }, '')
    return message
  }

  // Log a warning message with blue formatting
  warn (strings, ...values) {
    log.warn('trust', this.warnString(strings, ...values))
  }

  // dialogue is non-log text that is different from our usual npm prefix logging
  // it should always show to the user unless --json is specified
  // it's not controled by log levels
  dialogue (strings, ...values) {
    const json = this.config.get('json')
    if (!json) {
      output.standard(this.warnString(strings, ...values))
    }
  }

  createConfig (pkg, body) {
    const spec = npa(pkg)
    const uri = `/-/package/${spec.escapedName}/trust`
    return otplease(this.npm, this.npm.flatOptions, opts => npmFetch(uri, {
      ...opts,
      method: 'POST',
      body: body,
    }))
  }

  logOptions (options, pad = true) {
    const { values, warnings, fromPackageJson, urls } = { warnings: [], ...options }
    if (warnings && warnings.length > 0) {
      for (const warningMsg of warnings) {
        log.warn('trust', warningMsg)
      }
    }

    const json = this.config.get('json')
    if (json) {
      // Disable redaction: trust config values (e.g. CircleCI UUIDs) are not secrets
      output.standard(JSON.stringify(options.values, null, 2), { [META]: true, redact: false })
      return
    }

    const chalk = this.npm.chalk
    const { type, id, ...rest } = values || {}

    if (values) {
      const lines = []
      if (type) {
        lines.push(`type: ${chalk.green(type)}`)
      }
      if (id) {
        lines.push(`id: ${chalk.green(id)}`)
      }
      for (const [key, value] of Object.entries(rest)) {
        if (value !== null && value !== undefined) {
          const parts = [
            `${chalk.reset(key)}: ${chalk.green(value)}`,
          ]
          if (fromPackageJson && fromPackageJson[key]) {
            parts.push(`(${chalk.yellow(`from package.json`)})`)
          }
          lines.push(parts.join(' '))
        }
      }
      if (pad) {
        output.standard()
      }
      output.standard(lines.join('\n'), { [META]: true, redact: false })
      // Print URLs on their own lines after config, following the same order as rest keys
      if (urls) {
        const urlLines = []
        for (const key of Object.keys(rest)) {
          if (urls[key]) {
            urlLines.push(chalk.blue(urls[key]))
          }
        }
        if (urlLines.length > 0) {
          output.standard()
          output.standard(urlLines.join('\n'), { [META]: true, redact: false })
        }
      }
      if (pad) {
        output.standard()
      }
    }
  }

  async confirmOperation (yes) {
    // Ask for confirmation unless --yes flag is set
    if (yes === true) {
      return
    }
    if (yes === false) {
      throw new Error('User cancelled operation')
    }
    const confirm = await input.read(
      () => _read({ prompt: 'Do you want to proceed? (y/N) ', default: 'n' })
    )
    const normalized = confirm.toLowerCase()
    if (['y', 'yes'].includes(normalized)) {
      return
    }
    throw new Error('User cancelled operation')
  }

  getFrontendUrl ({ pkgName }) {
    if (this.registryIsDefault) {
      return new URL(`/package/${pkgName}`, NPM_FRONTEND).toString()
    }
    return null
  }

  getRepositoryFromPackageJson (pkg) {
    const info = gitinfo.fromUrl(pkg.repository?.url || pkg?.repository)
    if (!info) {
      return null
    }
    const repository = info.user + '/' + info.project
    const type = info.type
    return { repository, type }
  }

  async optionalPkgJson () {
    try {
      const { content } = await pkgJson.normalize(this.npm.prefix)
      return content
    } catch (err) {
      return {}
    }
  }

  get registryIsDefault () {
    return this.npm.config.defaults.registry === this.npm.config.get('registry')
  }

  // generic
  static bodyToOptions (body) {
    return {
      ...(body.id) && { id: body.id },
      ...(body.type) && { type: body.type },
    }
  }

  async createConfigCommand ({ positionalArgs, flags }) {
    const { providerName, providerEntity, providerHostname } = this.constructor
    const dryRun = this.config.get('dry-run')
    const yes = this.config.get('yes') // deep-lore this allows for --no-yes
    const options = await this.flagsToOptions({ positionalArgs, flags, providerHostname })
    this.dialogue`Establishing trust between ${options.values.package} package and ${providerName}`
    this.dialogue`Anyone with ${providerEntity} write access can publish to ${options.values.package}`
    this.dialogue`Two-factor authentication is required for this operation`
    if (!this.registryIsDefault) {
      this.warn`Registry ${this.npm.config.get('registry')} may not support trusted publishing`
    }
    this.logOptions(options)
    if (dryRun) {
      return
    }
    await this.confirmOperation(yes)
    const trustConfig = this.constructor.optionsToBody(options.values)
    const response = await this.createConfig(options.values.package, [trustConfig])
    const body = await response.json()
    this.dialogue`Trust configuration created successfully for ${options.values.package} with the following settings:`
    this.displayResponseBody({ body, packageName: options.values.package })
  }

  async flagsToOptions ({ positionalArgs, flags, providerHostname }) {
    const { entityKey, name, providerEntity, providerFile } = this.constructor
    const content = await this.optionalPkgJson()
    const pkgPositional = positionalArgs[0]
    const pkgJsonName = content.name
    const git = this.getRepositoryFromPackageJson(content)
    // the provided positional matches package.json name or no positional provided
    const matchPkg = (!pkgPositional || pkgPositional === pkgJsonName)
    const pkgName = pkgPositional || pkgJsonName
    const usedPkgNameFromPkgJson = !pkgPositional && Boolean(pkgJsonName)
    const invalidPkgJsonProviderType = matchPkg && git && git?.type !== name

    let entity
    let entitySource

    if (flags[entityKey]) {
      entity = flags[entityKey]
      entitySource = 'flag'
    } else if (!invalidPkgJsonProviderType && git?.repository) {
      entity = git.repository
      entitySource = 'package.json'
    }
    const mismatchPkgJsonRepository = matchPkg && git && entity !== git.repository
    const usedRepositoryInPkgJson = entitySource === 'package.json'

    const warnings = []
    if (!pkgName) {
      throw new Error('Package name must be specified either as an argument or in package.json file')
    }

    if (!flags.file) {
      throw new Error(`${providerFile} must be specified with the file option`)
    }
    if (!flags.file.endsWith('.yml') && !flags.file.endsWith('.yaml')) {
      throw new Error(`${providerFile} must end in .yml or .yaml`)
    }

    this.validateFile?.(flags.file)

    if (invalidPkgJsonProviderType) {
      const message = this.warnString`Repository in package.json is not a ${providerEntity}`
      if (!flags[entityKey]) {
        throw new Error(message)
      } else {
        warnings.push(message)
      }
    } else {
      if (mismatchPkgJsonRepository) {
        warnings.push(this.warnString`Repository in package.json (${git.repository}) differs from provided ${providerEntity} (${entity})`)
      }
    }

    if (!entity && matchPkg) {
      throw new Error(`${providerEntity} must be specified with ${entityKey} option or inferred from the package.json repository field`)
    }
    if (!entity) {
      throw new Error(`${providerEntity} must be specified with ${entityKey} option`)
    }

    this.validateEntity(entity)

    return {
      values: {
        package: pkgName,
        file: flags.file,
        [entityKey]: entity,
        ...(flags.environment && { environment: flags.environment }),
      },
      fromPackageJson: {
        [entityKey]: usedRepositoryInPkgJson,
        package: usedPkgNameFromPkgJson,
      },
      warnings: warnings,
      urls: {
        package: this.getFrontendUrl({ pkgName }),
        [entityKey]: this.getEntityUrl({ providerHostname, entity }),
        file: this.getEntityUrl({ providerHostname, entity, file: flags.file }),
      },
    }
  }

  displayResponseBody ({ body, packageName }) {
    if (!body || body.length === 0) {
      this.dialogue`No trust configurations found for package (${packageName})`
      return
    }
    const items = Array.isArray(body) ? body : [body]
    for (const config of items) {
      const values = this.constructor.bodyToOptions(config)
      output.standard()
      this.logOptions({ values }, false)
    }
    output.standard()
  }
}

module.exports = TrustCommand
module.exports.NPM_FRONTEND = NPM_FRONTEND
