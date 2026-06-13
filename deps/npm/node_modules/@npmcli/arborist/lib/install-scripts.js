const { isNodeGypPackage } = require('@npmcli/node-gyp')
const PackageJson = require('@npmcli/package-json')

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

  // Lockfile-only nodes carry `hasInstallScript: true` but no enumerated
  // scripts: the lockfile records the presence flag, not the script bodies,
  // so `node.package.scripts` is empty on a lockfile-driven install (`npm ci`,
  // a repeat `npm install`). Before giving up, read the installed
  // package.json from disk to recover the real script bodies. Builder#addToBuildSet
  // does the same disk read to decide what to run, but unlike that path this
  // one is read-only: we never mutate `node.package`.
  if (Object.keys(collected).length === 0 && node.hasInstallScript === true) {
    const { content } = await PackageJson.normalize(node.path)
      .catch(() => ({ content: {} }))
    /* istanbul ignore next: normalize resolves to an object with a scripts
       object, or our catch fallback returns {}; defensive guard only. */
    const diskScripts = content?.scripts || {}

    if (diskScripts.preinstall) {
      collected.preinstall = diskScripts.preinstall
    }
    if (diskScripts.install) {
      collected.install = diskScripts.install
    }
    if (diskScripts.postinstall) {
      collected.postinstall = diskScripts.postinstall
    }
    if (diskScripts.prepare && hasNonRegistryShape(node)) {
      collected.prepare = diskScripts.prepare
    }

    // Still nothing. The package isn't on disk yet (e.g. `npm ci` before
    // reify) or its package.json is unreadable. Emit a sentinel so the
    // advisory and the strict-allow-scripts preflight still surface that
    // install scripts are present.
    if (Object.keys(collected).length === 0) {
      collected.install = '(install scripts present)'
    }
  }

  return collected
}

module.exports = getInstallScripts
module.exports.getInstallScripts = getInstallScripts
