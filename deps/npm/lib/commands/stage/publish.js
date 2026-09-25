const Publish = require('../publish.js')

class StagePublish extends Publish {
  static description = 'Stage a package for publishing, deferring proof-of-presence (2FA) to a later point in time'
  static name = 'publish'
  static stage = true
  static params = Publish.params
  static usage = Publish.usage
  static workspaces = true
  static ignoreImplicitWorkspace = false
}

module.exports = StagePublish
