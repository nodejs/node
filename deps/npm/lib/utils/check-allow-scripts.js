const isScriptAllowed = require('@npmcli/arborist/lib/script-allowed.js')
const getInstallScripts = require('@npmcli/arborist/lib/install-scripts.js')

// Walks arb.actualTree.inventory and returns the list of dep nodes that
// have install-relevant lifecycle scripts and are not yet covered (or
// explicitly denied) by the allowScripts policy.
//
// Returns an array of `{ node, scripts }` entries. `scripts` is an object
// describing the relevant lifecycle scripts that would run.

const checkAllowScripts = async ({ arb, npm, tree }) => {
  const ignoreScripts = !!arb.options?.ignoreScripts
  const dangerouslyAllowAll = !!npm?.flatOptions?.dangerouslyAllowAllScripts

  if (ignoreScripts || dangerouslyAllowAll) {
    return []
  }

  // Defaults to actualTree (post-reify) but accepts an explicit tree so
  // callers can pre-flight against the idealTree before scripts run.
  const targetTree = tree || arb.actualTree
  if (!targetTree?.inventory) {
    return []
  }

  const policy = arb.options?.allowScripts || null

  const unreviewed = []
  for (const node of targetTree.inventory.values()) {
    if (node.isProjectRoot || node.isWorkspace) {
      continue
    }
    if (node.isLink) {
      // Linked workspace dependencies are managed by the workspace owner.
      continue
    }

    const verdict = isScriptAllowed(node, policy)
    if (verdict === true || verdict === false) {
      continue
    }

    const scripts = await getInstallScripts(node)
    if (Object.keys(scripts).length === 0) {
      continue
    }

    unreviewed.push({ node, scripts })
  }

  return unreviewed
}

module.exports = checkAllowScripts
