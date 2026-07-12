const { log, output, META } = require('proc-log')
const npmFetch = require('npm-registry-fetch')
const { otplease } = require('../../utils/auth.js')
const { validateUUID } = require('../../utils/validate-uuid.js')
const BaseCommand = require('../../base-cmd.js')

class StageApprove extends BaseCommand {
  static description = 'Approve a staged package, publishing it to the npm registry'
  static name = 'approve'
  static usage = ['<stage-id>']
  static params = ['otp', 'registry']
  static positionals = 1

  async exec (args) {
    if (!args[0]) {
      throw this.usageError('Missing required <stage-id>')
    }
    const stageId = args[0]
    validateUUID(stageId, 'stage-id')
    const opts = { ...this.npm.flatOptions }

    log.notice('', `Approving staged package ${stageId}`)

    await otplease(this.npm, opts, o =>
      npmFetch.json(`/-/stage/${stageId}/approve`, {
        ...o,
        method: 'POST',
      })
    )

    output.standard(`Staged package ${stageId} approved and published successfully.`, { [META]: true, redact: false })
  }
}

module.exports = StageApprove
