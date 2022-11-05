const log = require('./utils/log-shim.js')

// This is the base for all commands whose execWorkspaces just gets
// a list of workspace names and passes it on to new Arborist() to
// be able to run a filtered Arborist.reify() at some point.

const BaseCommand = require('./base-command.js')
class ArboristCmd extends BaseCommand {
  get isArboristCmd () {
    return true
  }

  static params = [
    'workspace',
    'workspaces',
    'include-workspace-root',
    'install-links',
  ]

  static ignoreImplicitWorkspace = false

  constructor (npm) {
    super(npm)
    if (this.npm.config.isDefault('audit')
      && (this.npm.global || this.npm.config.get('location') !== 'project')
    ) {
      this.npm.config.set('audit', false)
    } else if (this.npm.global && this.npm.config.get('audit')) {
      log.warn('config',
        'includes both --global and --audit, which is currently unsupported.')
    }
  }

  async execWorkspaces (args, filters) {
    await this.setWorkspaces(filters)
    return this.exec(args)
  }
}

module.exports = ArboristCmd
