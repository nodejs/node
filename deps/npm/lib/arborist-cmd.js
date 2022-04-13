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
  ]

  static ignoreImplicitWorkspace = false

  async execWorkspaces (args, filters) {
    await this.setWorkspaces(filters)
    return this.exec(args)
  }
}

module.exports = ArboristCmd
