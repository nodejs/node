// This is the base for all commands whose execWorkspaces just gets
// a list of workspace names and passes it on to new Arborist() to
// be able to run a filtered Arborist.reify() at some point.

const BaseCommand = require('../base-command.js')
class ArboristCmd extends BaseCommand {
  /* istanbul ignore next - see test/lib/load-all-commands.js */
  static get params () {
    return [
      'workspace',
      'workspaces',
    ]
  }

  execWorkspaces (args, filters, cb) {
    this.setWorkspaces(filters)
      .then(() => {
        this.exec(args, cb)
      })
      .catch(er => cb(er))
  }
}

module.exports = ArboristCmd
