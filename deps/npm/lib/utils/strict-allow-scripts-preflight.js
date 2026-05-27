const checkAllowScripts = require('./check-allow-scripts.js')

// Pre-flight check for `--strict-allow-scripts`. Call after arborist has
// been constructed but before `arb.reify()` runs, so that install scripts
// never execute when strict mode would block them.
//
// Behaviour:
//   - No-op unless `npm.flatOptions.strictAllowScripts` is set.
//   - Bypassed by `--dangerously-allow-all-scripts` and `--ignore-scripts`
//     (the per-flag short-circuits already live in checkAllowScripts).
//   - Builds the ideal tree (idempotent — subsequent reify reuses it),
//     walks it for nodes whose install scripts have not been covered by
//     the `allowScripts` policy, and throws if any are found.
const strictAllowScriptsPreflight = async ({ arb, npm, idealTreeOpts }) => {
  if (!npm?.flatOptions?.strictAllowScripts) {
    return
  }

  // Prefer the idealTree when reify is about to run; fall back to
  // actualTree for npm rebuild (which never builds an ideal tree).
  let tree
  if (idealTreeOpts !== undefined) {
    // `npm ci` builds the ideal tree before calling the preflight, so
    // skip the rebuild when one already exists. `npm install` calls the
    // preflight before reify and needs us to build.
    if (!arb.idealTree) {
      await arb.buildIdealTree(idealTreeOpts)
    }
    tree = arb.idealTree
  } else {
    tree = arb.actualTree
  }

  const unreviewed = await checkAllowScripts({ arb, npm, tree })
  if (unreviewed.length === 0) {
    return
  }

  const lines = unreviewed.map(({ node, scripts }) => {
    const events = Object.entries(scripts)
      .map(([event, body]) => `${event}: ${body}`)
      .join('; ')
    const name = node.package?.name || node.name
    const version = node.package?.version || ''
    const label = version ? `${name}@${version}` : name
    return `  ${label} (${events})`
  }).join('\n')

  throw Object.assign(
    new Error(
      `--strict-allow-scripts: ${unreviewed.length} package(s) have install ` +
      `scripts not covered by allowScripts:\n${lines}\n` +
      'Approve them with `npm approve-scripts`, deny them with ' +
      '`npm deny-scripts`, or bypass this check with ' +
      '`--dangerously-allow-all-scripts`.'
    ),
    { code: 'ESTRICTALLOWSCRIPTS' }
  )
}

module.exports = strictAllowScriptsPreflight
