const mapWorkspaces = require('@npmcli/map-workspaces')

// shared ref used by other mixins/Arborist
const _setWorkspaces = Symbol.for('setWorkspaces')

module.exports = cls => class MapWorkspaces extends cls {
  async [_setWorkspaces] (node) {
    const workspaces = await mapWorkspaces({
      cwd: node.path,
      pkg: node.package,
    })

    if (node && workspaces.size) {
      node.workspaces = workspaces
    }

    return node
  }
}
