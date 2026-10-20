const { log, output, META } = require('proc-log')
const npmFetch = require('npm-registry-fetch')
const { otplease } = require('../../utils/auth.js')
const { validateUUID } = require('../../utils/validate-uuid.js')
const BaseCommand = require('../../base-cmd.js')

class StageReject extends BaseCommand {
  static description = 'Reject a staged package, removing it from the registry'
  static name = 'reject'
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

    log.notice('', `Rejecting staged package ${stageId}`)
    log.warn('', 'Rejecting will permanently delete this staged publish record and tarball from the registry.')

    await otplease(this.npm, opts, o =>
      npmFetch(`/-/stage/${stageId}`, {
        ...o,
        method: 'DELETE',
        ignoreBody: true,
      })
    )

    output.standard(`Staged package ${stageId} has been rejected.`, { [META]: true, redact: false })
  }
}

module.exports = StageReject
