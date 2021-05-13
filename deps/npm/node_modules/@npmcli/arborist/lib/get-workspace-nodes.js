// Get the actual nodes corresponding to a root node's child workspaces,
// given a list of workspace names.
const relpath = require('./relpath.js')
const getWorkspaceNodes = (tree, workspaces, log) => {
  const wsMap = tree.workspaces
  if (!wsMap) {
    log.warn('workspaces', 'filter set, but no workspaces present')
    return []
  }

  const nodes = []
  for (const name of workspaces) {
    const path = wsMap.get(name)
    if (!path) {
      log.warn('workspaces', `${name} in filter set, but not in workspaces`)
      continue
    }

    const loc = relpath(tree.realpath, path)
    const node = tree.inventory.get(loc)

    if (!node) {
      log.warn('workspaces', `${name} in filter set, but no workspace folder present`)
      continue
    }

    nodes.push(node)
  }

  return nodes
}

module.exports = getWorkspaceNodes
