const { isNodeGypPackage } = require('@npmcli/node-gyp')

// Returns the install-relevant lifecycle scripts that would run for a
// given arborist Node, or `{}` if there are none.
//
// Includes:
//   - explicit preinstall/install/postinstall
//   - prepare, but only for non-registry sources (git, file, link, remote)
//   - synthetic `node-gyp rebuild`, when `binding.gyp` is present on disk
//     and the package does not opt out via `gypfile: false` or define its
//     own install / preinstall script

// Lifecycle-script enumeration boundary.
//
// IMPORTANT: this helper decides whether `prepare` should be included
// in the enumerated install scripts (true for non-registry sources only).
// It is NOT a policy-matching predicate. The policy matcher in
// script-allowed.js uses `isRegistryNode`, which is strictly tied to
// versionFromTgz(node.resolved). The two helpers exist separately on
// purpose:
//
//   - `hasNonRegistryShape` (here): "should we consider running prepare
//     on this node?" — a yes/no for what to enumerate.
//   - `isRegistryNode` (script-allowed.js): "do we trust this node's
//     identity enough to apply a policy entry?" — a security check.
//
// The looser fallback here (treating unknown-resolved nodes as registry,
// thus skipping `prepare`) is the safer default for enumeration: we'd
// rather omit a script we should have run than synthesise one for a
// non-registry source we couldn't confirm. The policy matcher's stricter
// behaviour is correct for its boundary; the two helpers must not be
// merged.
const hasNonRegistryShape = (node) => {
  if (typeof node.isRegistryDependency === 'boolean') {
    return !node.isRegistryDependency
  }
  if (!node.resolved) {
    return false
  }
  return !/^https?:\/\/[^/]+\/.+\/-\/[^/]+-\d/.test(node.resolved)
}

const getInstallScripts = async (node) => {
  /* istanbul ignore next: arborist Nodes always carry a `package` object;
     defensive fallbacks for non-arborist callers. */
  const pkg = node.package || {}
  /* istanbul ignore next */
  const scripts = pkg.scripts || {}
  const collected = {}

  if (scripts.preinstall) {
    collected.preinstall = scripts.preinstall
  }
  if (scripts.install) {
    collected.install = scripts.install
  }
  if (scripts.postinstall) {
    collected.postinstall = scripts.postinstall
  }
  if (scripts.prepare && hasNonRegistryShape(node)) {
    collected.prepare = scripts.prepare
  }

  const hasExplicitGypGate = !!(collected.preinstall || collected.install)
  if (
    !hasExplicitGypGate &&
    pkg.gypfile !== false &&
    await isNodeGypPackage(node.path).catch(() => false)
  ) {
    collected.install = 'node-gyp rebuild'
  }

  // Lockfile-only nodes (e.g. `npm ci` before reify) carry
  // `hasInstallScript: true` but no enumerated scripts: the lockfile
  // records the presence flag but never the script bodies. Without this
  // fallback the strict-allow-scripts preflight would miss them entirely
  // and let postinstall run. We can't recover the real script body
  // without fetching the manifest, so emit a sentinel describing that
  // install scripts are present.
  if (Object.keys(collected).length === 0 && node.hasInstallScript === true) {
    collected.install = '(install scripts present)'
  }

  return collected
}

module.exports = getInstallScripts
module.exports.getInstallScripts = getInstallScripts
