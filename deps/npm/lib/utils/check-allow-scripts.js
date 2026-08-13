const { collectUnreviewedScripts } = require('@npmcli/arborist/lib/unreviewed-scripts.js')

// Walks a tree's inventory and returns the list of dep nodes that have
// install-relevant lifecycle scripts and are not yet covered (or explicitly
// denied) by the allowScripts policy.
//
// Thin wrapper around arborist's shared `collectUnreviewedScripts`, mapping
// the CLI's `({ arb, npm, tree })` shape onto the shared walk. Defaults to
// `arb.actualTree` (post-reify) but accepts an explicit tree so callers can
// pre-flight against the idealTree before scripts run.
//
// Returns an array of `{ node, scripts }` entries. `scripts` is an object
// describing the relevant lifecycle scripts that would run.
//
// `includeWhenIgnored` keeps listing unreviewed packages even when
// ignore-scripts is set, so approve/deny can show what you'd move from a
// blanket ignore-scripts to an allowlist. Execution callers leave it false.
const checkAllowScripts = async ({ arb, npm, tree, includeWhenIgnored = false }) =>
  collectUnreviewedScripts({
    tree: tree || arb.actualTree,
    policy: arb.options?.allowScripts || null,
    ignoreScripts: !!arb.options?.ignoreScripts,
    dangerouslyAllowAllScripts: !!npm?.flatOptions?.dangerouslyAllowAllScripts,
    includeWhenIgnored,
  })

module.exports = checkAllowScripts
