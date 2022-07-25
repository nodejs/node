const mapWorkspaces = require('@npmcli/map-workspaces')

const _appendWorkspaces = Symbol('appendWorkspaces')
// shared ref used by other mixins/Arborist
const _loadWorkspaces = Symbol.for('loadWorkspaces')
const _loadWorkspacesVirtual = Symbol.for('loadWorkspacesVirtual')

module.exports = cls => class MapWorkspaces extends cls {
  [_appendWorkspaces] (node, workspaces) {
    if (node && workspaces.size) {
      node.workspaces = workspaces
    }

    return node
  }

  async [_loadWorkspaces] (node) {
    if (node.workspaces) {
      return node
    }

    const workspaces = await mapWorkspaces({
      cwd: node.path,
      pkg: node.package,
    })

    return this[_appendWorkspaces](node, workspaces)
  }

  [_loadWorkspacesVirtual] (opts) {
    return mapWorkspaces.virtual(opts)
  }
}
