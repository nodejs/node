const Definition = require('@npmcli/config/lib/definitions/definition.js')
const globalDefinitions = require('@npmcli/config/lib/definitions/definitions.js')
const TrustCommand = require('../../trust-cmd.js')
const path = require('node:path')

class TrustGitHub extends TrustCommand {
  static description = 'Create a trusted relationship between a package and GitHub Actions'
  static name = 'github'
  static positionals = 1 // expects at most 1 positional (package name)
  static providerName = 'GitHub Actions'
  static providerEntity = 'GitHub repository'
  static providerFile = 'GitHub Actions Workflow'
  static providerHostname = 'https://github.com'

  // entity means project / repository
  static entityKey = 'repository'

  static usage = [
    '[package] --file [--repo|--repository] [--env|--environment] [-y|--yes]',
  ]

  static definitions = [
    new Definition('file', {
      default: null,
      type: String,
      required: true,
      description: 'Name of workflow file within a repositories .GitHub folder (must end in yaml, yml)',
    }),
    new Definition('repository', {
      default: null,
      type: String,
      description: 'Name of the repository in the format owner/repo',
      alias: ['repo'],
    }),
    new Definition('environment', {
      default: null,
      type: String,
      description: 'CI environment name',
      alias: ['env'],
    }),
    // globals are alphabetical
    globalDefinitions['dry-run'],
    globalDefinitions.json,
    globalDefinitions.registry,
    globalDefinitions.yes,
  ]

  getEntityUrl ({ providerHostname, file, entity }) {
    if (file) {
      return new URL(`${entity}/blob/HEAD/.github/workflows/${file}`, providerHostname).toString()
    }
    return new URL(entity, providerHostname).toString()
  }

  validateEntity (entity) {
    if (entity.split('/').length !== 2) {
      throw new Error(`${this.constructor.providerEntity} must be specified in the format owner/repository`)
    }
  }

  validateFile (file) {
    if (file !== path.basename(file)) {
      throw new Error('GitHub Actions workflow must be just a file not a path')
    }
  }

  static optionsToBody (options) {
    const { file, repository, environment } = options
    const trustConfig = {
      type: 'github',
      claims: {
        repository,
        workflow_ref: {
          file,
        },
        ...(environment) && { environment },
      },
    }
    return trustConfig
  }

  // Convert API response body to options
  static bodyToOptions (body) {
    const file = body.claims?.workflow_ref?.file
    const repository = body.claims?.repository
    const environment = body.claims?.environment
    return {
      ...(body.id) && { id: body.id },
      ...(body.type) && { type: body.type },
      ...(file) && { file },
      ...(repository) && { repository },
      ...(environment) && { environment },
    }
  }

  async exec (positionalArgs, flags) {
    await this.createConfigCommand({
      positionalArgs,
      flags,
    })
  }
}

module.exports = TrustGitHub
