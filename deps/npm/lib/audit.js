const Arborist = require('@npmcli/arborist')
const auditReport = require('npm-audit-report')
const reifyFinish = require('./utils/reify-finish.js')
const auditError = require('./utils/audit-error.js')
const BaseCommand = require('./base-command.js')

class Audit extends BaseCommand {
  /* istanbul ignore next - see test/lib/load-all-commands.js */
  static get name () {
    return 'audit'
  }

  /* istanbul ignore next - see test/lib/load-all-commands.js */
  static get usage () {
    return [
      '[--json] [--production]',
      'fix [--force|--package-lock-only|--dry-run|--production|--only=(dev|prod)]',
    ]
  }

  async completion (opts) {
    const argv = opts.conf.argv.remain

    if (argv.length === 2)
      return ['fix']

    switch (argv[2]) {
      case 'fix':
        return []
      default:
        throw new Error(argv[2] + ' not recognized')
    }
  }

  exec (args, cb) {
    this.audit(args).then(() => cb()).catch(cb)
  }

  async audit (args) {
    const arb = new Arborist({
      ...this.npm.flatOptions,
      audit: true,
      path: this.npm.prefix,
    })
    const fix = args[0] === 'fix'
    await arb.audit({ fix })
    if (fix)
      await reifyFinish(this.npm, arb)
    else {
      // will throw if there's an error, because this is an audit command
      auditError(this.npm, arb.auditReport)
      const reporter = this.npm.flatOptions.json ? 'json' : 'detail'
      const result = auditReport(arb.auditReport, {
        ...this.npm.flatOptions,
        reporter,
      })
      process.exitCode = process.exitCode || result.exitCode
      this.npm.output(result.report)
    }
  }
}

module.exports = Audit
