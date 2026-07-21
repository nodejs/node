const { collectUnreviewedScripts, strictAllowScriptsError } = require('@npmcli/arborist/lib/unreviewed-scripts.js')

// Strict-mode pre-flight for `npm exec` / `npx`. When
// `--strict-allow-scripts` is set, build the npx-cache ideal tree and
// throw before reify if any node has install scripts not covered by
// the resolved `allowScripts` policy. The arborist gate already
// silently skips those scripts; this turns the silent skip into a
// hard failure for CI. Bypassed by `--ignore-scripts` and
// `--dangerously-allow-all-scripts`.
const strictAllowScriptsPreflight = async (arb, opts) => {
  if (!opts.strictAllowScripts) {
    return
  }
  if (opts.ignoreScripts || opts.dangerouslyAllowAllScripts) {
    return
  }

  if (!arb.idealTree) {
    await arb.buildIdealTree(opts)
  }

  const unreviewed = await collectUnreviewedScripts({
    tree: arb.idealTree,
    policy: opts.allowScripts || null,
    ignoreScripts: opts.ignoreScripts,
    dangerouslyAllowAllScripts: opts.dangerouslyAllowAllScripts,
  })

  if (unreviewed.length === 0) {
    return
  }

  throw strictAllowScriptsError(unreviewed, {
    remediation:
      'Pass --allow-scripts=<pkg> for one-off approval, or bypass this ' +
      'check with --dangerously-allow-all-scripts.',
  })
}

module.exports = strictAllowScriptsPreflight
