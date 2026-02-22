const Definition = require('@npmcli/config/lib/definitions/definition.js')
const globalDefinitions = require('@npmcli/config/lib/definitions/definitions.js')
const TrustCommand = require('../../trust-cmd.js')
const path = require('node:path')

class TrustGitLab extends TrustCommand {
  static description = 'Create a trusted relationship between a package and GitLab CI/CD'
  static name = 'gitlab'
  static positionals = 1 // expects at most 1 positional (package name)
  static providerName = 'GitLab CI/CD'
  static providerEntity = 'GitLab project'
  static providerFile = 'GitLab CI/CD Pipeline'
  static providerHostname = 'https://gitlab.com'

  // entity means project / repository
  static entityKey = 'project'

  static usage = [
    '[package] --file [--project|--repo|--repository] [--env|--environment] [-y|--yes]',
  ]

  static definitions = [
    new Definition('file', {
      default: null,
      type: String,
      required: true,
      description: 'Name of pipeline file (e.g., .gitlab-ci.yml)',
    }),
    new Definition('project', {
      default: null,
      type: String,
      description: 'Name of the project in the format group/project or group/subgroup/project',
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
      return new URL(`${entity}/-/blob/HEAD/${file}`, providerHostname).toString()
    }
    return new URL(entity, providerHostname).toString()
  }

  validateEntity (entity) {
    if (entity.split('/').length < 2) {
      throw new Error(`${this.constructor.providerEntity} must be specified in the format group/project or group/subgroup/project`)
    }
  }

  validateFile (file) {
    if (file !== path.basename(file)) {
      throw new Error('GitLab CI/CD pipeline file must be just a file not a path')
    }
  }

  static optionsToBody (options) {
    const { file, project, environment } = options
    const trustConfig = {
      type: 'gitlab',
      claims: {
        project_path: project,
        // this looks off, but this is correct
        /** The ref path to the top-level pipeline definition, for example, gitlab.example.com/my-group/my-project//.gitlab-ci.yml@refs/heads/main. Introduced in GitLab 16.2. This claim is null unless the pipeline definition is located in the same project. */
        ci_config_ref_uri: {
          file,
        },
        ...(environment) && { environment },
      },
    }
    return trustConfig
  }

  // Convert API response body to options
  static bodyToOptions (body) {
    const file = body.claims?.ci_config_ref_uri?.file
    const project = body.claims?.project_path
    const environment = body.claims?.environment
    return {
      ...(body.id) && { id: body.id },
      ...(body.type) && { type: body.type },
      ...(file) && { file },
      ...(project) && { project },
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

module.exports = TrustGitLab
