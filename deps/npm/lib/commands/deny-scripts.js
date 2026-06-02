const AllowScriptsCmd = require('../utils/allow-scripts-cmd.js')

class DenyScripts extends AllowScriptsCmd {
  static description = 'Deny install scripts for specific dependencies'
  static name = 'deny-scripts'
  static usage = ['<pkg> [<pkg> ...]', '--all']
  static verb = 'deny'
}

module.exports = DenyScripts
