const { output, META } = require('proc-log')
const npmFetch = require('npm-registry-fetch')
const { logStageItem } = require('../../utils/key-values.js')
const { validateUUID } = require('../../utils/validate-uuid.js')
const BaseCommand = require('../../base-cmd.js')

class StageView extends BaseCommand {
  static description = 'View details of a specific staged package'
  static name = 'view'
  static usage = ['<stage-id>']
  static params = ['json', 'registry']
  static positionals = 1

  async exec (args) {
    if (!args[0]) {
      throw this.usageError('Missing required <stage-id>')
    }
    const stageId = args[0]
    validateUUID(stageId, 'stage-id')
    const opts = { ...this.npm.flatOptions }
    const json = this.npm.config.get('json')

    const item = await npmFetch.json(`/-/stage/${stageId}`, opts)

    if (json) {
      output.standard(JSON.stringify(item, null, 2), { [META]: true, redact: false })
      return
    }

    logStageItem(item, { chalk: this.npm.chalk })
  }
}

module.exports = StageView
