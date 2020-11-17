const Arborist = require('@npmcli/arborist')
const auditReport = require('npm-audit-report')
const npm = require('./npm.js')
const output = require('./utils/output.js')
const reifyFinish = require('./utils/reify-finish.js')
const auditError = require('./utils/audit-error.js')

const audit = async args => {
  const arb = new Arborist({
    ...npm.flatOptions,
    audit: true,
    path: npm.prefix,
  })
  const fix = args[0] === 'fix'
  await arb.audit({ fix })
  if (fix)
    await reifyFinish(arb)
  else {
    // will throw if there's an error, because this is an audit command
    auditError(arb.auditReport)
    const reporter = npm.flatOptions.json ? 'json' : 'detail'
    const result = auditReport(arb.auditReport, {
      ...npm.flatOptions,
      reporter,
    })
    process.exitCode = process.exitCode || result.exitCode
    output(result.report)
  }
}

const cmd = (args, cb) => audit(args).then(() => cb()).catch(cb)

const usageUtil = require('./utils/usage')
const usage = usageUtil(
  'audit',
  'npm audit [--json] [--production]' +
  '\nnpm audit fix ' +
  '[--force|--package-lock-only|--dry-run|--production|--only=(dev|prod)]'
)

const completion = (opts, cb) => {
  const argv = opts.conf.argv.remain

  if (argv.length === 2)
    return cb(null, ['fix'])

  switch (argv[2]) {
    case 'fix':
      return cb(null, [])
    default:
      return cb(new Error(argv[2] + ' not recognized'))
  }
}

module.exports = Object.assign(cmd, { usage, completion })
