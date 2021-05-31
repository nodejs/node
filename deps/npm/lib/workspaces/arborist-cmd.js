// This is the base for all commands whose execWorkspaces just gets
// a list of workspace names and passes it on to new Arborist() to
// be able to run a filtered Arborist.reify() at some point.

const BaseCommand = require('../base-command.js')
const getWorkspaces = require('./get-workspaces.js')
class ArboristCmd extends BaseCommand {
  /* istanbul ignore next - see test/lib/load-all-commands.js */
  static get params () {
    return [
      'workspace',
      'workspaces',
    ]
  }

  execWorkspaces (args, filters, cb) {
    getWorkspaces(filters, { path: this.npm.localPrefix })
      .then(workspaces => {
        this.workspaces = [...workspaces.keys()]
        this.workspacePaths = [...workspaces.values()]
        this.exec(args, cb)
      })
      .catch(er => cb(er))
  }
}

module.exports = ArboristCmd
