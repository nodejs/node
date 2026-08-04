const { log } = require('proc-log')

// The allowScripts policy MUST live at the project root (RFC npm/rfcs#868).
// A non-root workspace declaring its own allowScripts is almost always a
// mistake: that policy would be silently ignored at install time.
//
// `findWorkspaceAllowScripts` returns the list of offending workspace nodes.
// `warnWorkspaceAllowScripts` is the side-effecting variant that emits one
// install-time `log.warn` per offender.

const findWorkspaceAllowScripts = (tree) => {
  const offenders = []
  if (!tree?.inventory) {
    return offenders
  }
  for (const node of tree.inventory.values()) {
    if (!node.isWorkspace || node.isProjectRoot) {
      continue
    }
    if (node.package?.allowScripts !== undefined) {
      offenders.push(node)
    }
  }
  return offenders
}

const warnWorkspaceAllowScripts = (tree) => {
  for (const node of findWorkspaceAllowScripts(tree)) {
    const name = node.packageName || node.name
    log.warn(
      'allow-scripts',
      `allowScripts in workspace ${name} (${node.path}) is ignored. ` +
      'Move the field to the project root package.json.'
    )
  }
}

module.exports = warnWorkspaceAllowScripts
module.exports.warnWorkspaceAllowScripts = warnWorkspaceAllowScripts
module.exports.findWorkspaceAllowScripts = findWorkspaceAllowScripts
