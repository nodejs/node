const isScriptAllowed = require('./script-allowed.js')
const getInstallScripts = require('./install-scripts.js')

// Shared allowScripts walk used by both the npm CLI
// (lib/utils/check-allow-scripts.js, lib/utils/strict-allow-scripts-preflight.js)
// and libnpmexec (npm exec / npx). It lives in arborist because that is the
// only package both callers can import.
//
// Walks a tree's inventory and returns the dep nodes that have
// install-relevant lifecycle scripts and are not yet covered (or explicitly
// denied) by the allowScripts policy.
//
// Returns an array of `{ node, scripts }` entries. `scripts` is an object
// describing the relevant lifecycle scripts that would run.
const collectUnreviewedScripts = async ({
  tree,
  policy,
  ignoreScripts = false,
  dangerouslyAllowAllScripts = false,
  includeWhenIgnored = false,
} = {}) => {
  // With ignore-scripts set, no scripts run, so execution callers bail out
  // here. approve/deny pass includeWhenIgnored so they keep listing
  // unreviewed packages, which is what you need to move from a blanket
  // ignore-scripts to an allowlist. Listing never runs anything.
  if ((ignoreScripts && !includeWhenIgnored) || dangerouslyAllowAllScripts) {
    return []
  }

  if (!tree?.inventory) {
    return []
  }

  const resolvedPolicy = policy || null

  const unreviewed = []
  for (const node of tree.inventory.values()) {
    if (node.isProjectRoot || node.isWorkspace) {
      continue
    }
    if (node.isLink) {
      // Linked workspace dependencies are managed by the workspace owner.
      continue
    }
    if (node.inBundle) {
      // Bundled dependencies never run their install scripts and cannot be
      // allowlisted, so they are never "pending". Skipping them keeps them
      // out of the advisory warning and out of strict-allow-scripts. A
      // package that needs a bundled dep's script must forward it as one of
      // its own lifecycle scripts.
      continue
    }
    if (node.inert) {
      // Inert = an optional dep that can't be installed here (failed the
      // os/cpu/libc or engine check, or failed to load). reify drops it
      // before any script runs, so its install scripts never execute and it
      // must not be flagged (npm/cli#9562).
      continue
    }

    const verdict = isScriptAllowed(node, resolvedPolicy)
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

// Builds the `ESTRICTALLOWSCRIPTS` error thrown by the strict-mode preflight
// from a list of `{ node, scripts }` entries. `remediation` is the
// caller-specific guidance appended after the package list (npm install vs
// npm exec have different remediation commands).
const strictAllowScriptsError = (unreviewed, { remediation } = {}) => {
  const lines = unreviewed.map(({ node, scripts }) => {
    const events = Object.entries(scripts)
      .map(([event, body]) => `${event}: ${body}`)
      .join('; ')
    const name = node.package?.name || node.name
    const version = node.package?.version || ''
    const label = version ? `${name}@${version}` : name
    return `  ${label} (${events})`
  }).join('\n')

  return Object.assign(
    new Error(
      `--strict-allow-scripts: ${unreviewed.length} package(s) have install ` +
      `scripts not covered by allowScripts:\n${lines}\n${remediation}`
    ),
    { code: 'ESTRICTALLOWSCRIPTS' }
  )
}

module.exports = { collectUnreviewedScripts, strictAllowScriptsError }
