const AllowScriptsCmd = require('../utils/allow-scripts-cmd.js')

// Namespaced front-end for install-script approvals.
// `approve`/`deny` write the `allowScripts` policy, `ls` lists unreviewed packages,
// `prune` drops entries that no longer match an installed package with an install script.
// `npm approve-scripts` / `npm deny-scripts` are aliases for `approve` / `deny`.
class InstallScripts extends AllowScriptsCmd {
  static description = 'Manage install-script approvals for dependencies'
  static name = 'install-scripts'
  static usage = [
    'approve <pkg> [<pkg> ...]',
    'approve --all',
    'deny <pkg> [<pkg> ...]',
    'deny --all',
    'ls',
    'prune',
  ]

  static params = ['all', 'allow-scripts-pin', 'dry-run', 'json']

  static async completion (opts) {
    const argv = opts.conf.argv.remain
    const subcommands = ['approve', 'deny', 'ls', 'prune']
    if (argv.length === 2) {
      return subcommands
    }
    if (subcommands.includes(argv[2])) {
      return []
    }
    throw new Error(`${argv[2]} not recognized`)
  }

  async exec (args) {
    const [sub, ...rest] = args
    switch (sub) {
      case 'approve':
        return this.runMode('approve', rest)
      case 'deny':
        return this.runMode('deny', rest)
      case 'ls':
      case 'list':
        return this.runMode('list', rest)
      case 'prune':
        return this.runMode('prune', rest)
      default:
        throw this.usageError(
          sub ? `\`${sub}\` is not a recognized subcommand.` : undefined
        )
    }
  }
}

module.exports = InstallScripts
