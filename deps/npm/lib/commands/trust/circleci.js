const Definition = require('@npmcli/config/lib/definitions/definition.js')
const globalDefinitions = require('@npmcli/config/lib/definitions/definitions.js')
const TrustCommand = require('../../trust-cmd.js')

// UUID validation regex
const UUID_REGEX = /^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$/i

class TrustCircleCI extends TrustCommand {
  static description = 'Create a trusted relationship between a package and CircleCI'
  static name = 'circleci'
  static positionals = 1 // expects at most 1 positional (package name)
  static providerName = 'CircleCI'
  static providerEntity = 'CircleCI pipeline'

  static usage = [
    '[package] --org-id <uuid> --project-id <uuid> --pipeline-definition-id <uuid> --vcs-origin <origin> [--context-id <uuid>...] [-y|--yes]',
  ]

  static definitions = [
    new Definition('org-id', {
      default: null,
      type: String,
      required: true,
      description: 'CircleCI organization UUID',
    }),
    new Definition('project-id', {
      default: null,
      type: String,
      required: true,
      description: 'CircleCI project UUID',
    }),
    new Definition('pipeline-definition-id', {
      default: null,
      type: String,
      required: true,
      description: 'CircleCI pipeline definition UUID',
    }),
    new Definition('vcs-origin', {
      default: null,
      type: String,
      required: true,
      description: "CircleCI repository origin in format 'provider/owner/repo'",
    }),
    new Definition('context-id', {
      default: null,
      type: [null, String, Array],
      description: 'CircleCI context UUID to match',
    }),
    // globals are alphabetical
    globalDefinitions['dry-run'],
    globalDefinitions.json,
    globalDefinitions.registry,
    globalDefinitions.yes,
  ]

  validateUuid (value, fieldName) {
    if (!UUID_REGEX.test(value)) {
      throw new Error(`${fieldName} must be a valid UUID`)
    }
  }

  validateVcsOrigin (value) {
    // Expected format: provider/owner/repo (e.g., github.com/owner/repo, bitbucket.org/owner/repo)
    if (value.includes('://')) {
      throw new Error("vcs-origin must not include a scheme (e.g., use 'github.com/owner/repo' not 'https://github.com/owner/repo')")
    }
    const parts = value.split('/')
    if (parts.length < 3) {
      throw new Error("vcs-origin must be in format 'provider/owner/repo'")
    }
  }

  // Generate a URL from vcs-origin (e.g., github.com/npm/repo -> https://github.com/npm/repo)
  getVcsOriginUrl (vcsOrigin) {
    if (!vcsOrigin) {
      return null
    }
    // vcs-origin format: github.com/owner/repo or bitbucket.org/owner/repo
    return `https://${vcsOrigin}`
  }

  static optionsToBody (options) {
    const { orgId, projectId, pipelineDefinitionId, vcsOrigin, contextIds } = options
    const trustConfig = {
      type: 'circleci',
      claims: {
        'oidc.circleci.com/org-id': orgId,
        'oidc.circleci.com/project-id': projectId,
        'oidc.circleci.com/pipeline-definition-id': pipelineDefinitionId,
        'oidc.circleci.com/vcs-origin': vcsOrigin,
      },
    }
    if (contextIds && contextIds.length > 0) {
      trustConfig.claims['oidc.circleci.com/context-ids'] = contextIds
    }
    return trustConfig
  }

  static bodyToOptions (body) {
    return {
      ...(body.id) && { id: body.id },
      ...(body.type) && { type: body.type },
      ...(body.claims?.['oidc.circleci.com/org-id']) && { orgId: body.claims['oidc.circleci.com/org-id'] },
      ...(body.claims?.['oidc.circleci.com/project-id']) && { projectId: body.claims['oidc.circleci.com/project-id'] },
      ...(body.claims?.['oidc.circleci.com/pipeline-definition-id']) && {
        pipelineDefinitionId: body.claims['oidc.circleci.com/pipeline-definition-id'],
      },
      ...(body.claims?.['oidc.circleci.com/vcs-origin']) && { vcsOrigin: body.claims['oidc.circleci.com/vcs-origin'] },
      ...(body.claims?.['oidc.circleci.com/context-ids']) && { contextIds: body.claims['oidc.circleci.com/context-ids'] },
    }
  }

  // Override flagsToOptions since CircleCI doesn't use file/entity pattern
  async flagsToOptions ({ positionalArgs, flags }) {
    const content = await this.optionalPkgJson()
    const pkgName = positionalArgs[0] || content.name

    if (!pkgName) {
      throw new Error('Package name must be specified either as an argument or in package.json file')
    }

    const orgId = flags['org-id']
    const projectId = flags['project-id']
    const pipelineDefinitionId = flags['pipeline-definition-id']
    const vcsOrigin = flags['vcs-origin']
    const contextIds = flags['context-id']

    // Validate required flags
    if (!orgId) {
      throw new Error('org-id is required')
    }
    if (!projectId) {
      throw new Error('project-id is required')
    }
    if (!pipelineDefinitionId) {
      throw new Error('pipeline-definition-id is required')
    }
    if (!vcsOrigin) {
      throw new Error('vcs-origin is required')
    }

    // Validate formats
    this.validateUuid(orgId, 'org-id')
    this.validateUuid(projectId, 'project-id')
    this.validateUuid(pipelineDefinitionId, 'pipeline-definition-id')
    this.validateVcsOrigin(vcsOrigin)
    if (contextIds?.length > 0) {
      for (const contextId of contextIds) {
        this.validateUuid(contextId, 'context-id')
      }
    }

    return {
      values: {
        package: pkgName,
        orgId,
        projectId,
        pipelineDefinitionId,
        vcsOrigin,
        ...(contextIds?.length > 0 && { contextIds }),
      },
      fromPackageJson: {},
      warnings: [],
      urls: {
        package: this.getFrontendUrl({ pkgName }),
        vcsOrigin: this.getVcsOriginUrl(vcsOrigin),
      },
    }
  }

  async exec (positionalArgs, flags) {
    await this.createConfigCommand({
      positionalArgs,
      flags,
    })
  }
}

module.exports = TrustCircleCI
