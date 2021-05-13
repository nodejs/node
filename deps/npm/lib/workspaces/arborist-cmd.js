// This is the base for all commands whose execWorkspaces just gets
// a list of workspace names and passes it on to new Arborist() to
// be able to run a filtered Arborist.reify() at some point.

const BaseCommand = require('../base-command.js')
const getWorkspaces = require('../workspaces/get-workspaces.js')
class ArboristCmd extends BaseCommand {
  /* istanbul ignore next - see test/lib/load-all-commands.js */
  static get params () {
    return [
      'workspace',
    ]
  }

  execWorkspaces (args, filters, cb) {
    getWorkspaces(filters, { path: this.npm.localPrefix })
      .then(workspaces => {
        this.workspaces = [...workspaces.keys()]
        this.exec(args, cb)
      })
  }
}

module.exports = ArboristCmd
