// compares the inventory of package items in the tree
// that is about to be installed (idealTree) with the inventory
// of items stored in the package-lock file (virtualTree)
//
// Returns empty array if no errors found or an array populated
// with an entry for each validation error found.
function validateLockfile (virtualTree, idealTree) {
  const errors = []

  // loops through the inventory of packages resulted by ideal tree,
  // for each package compares the versions with the version stored in the
  // package-lock and adds an error to the list in case of mismatches
  for (const [key, entry] of idealTree.entries()) {
    const lock = virtualTree.get(key)

    if (!lock) {
      errors.push(`Missing: ${entry.name}@${entry.version} from lock file`)
      continue
    }

    if (entry.version !== lock.version) {
      errors.push(`Invalid: lock file's ${lock.name}@${lock.version} does ` +
      `not satisfy ${entry.name}@${entry.version}`)
    }

    // a patch whose on-disk hash or path diverges from the lockfile is out of sync
    if ((lock.patched?.integrity || null) !== (entry.patched?.integrity || null) ||
      (lock.patched?.path || null) !== (entry.patched?.path || null)) {
      if (entry.patched && !lock.patched) {
        // package.json declares a patch the lockfile lacks: newly added, or skipped via --ignore-patch-failures
        errors.push(`Invalid: package.json declares a patch for ${entry.name}@${entry.version} ` +
        `that the lock file does not record (it may have been skipped with --ignore-patch-failures). ` +
        `Fix the patch and reinstall, or remove its patchedDependencies entry`)
      } else if (lock.patched && !entry.patched) {
        // describe the lock file's own version, which can differ from the ideal tree's when the version also drifted
        errors.push(`Invalid: lock file records a patch for ${lock.name}@${lock.version} ` +
        `that package.json no longer declares`)
      } else {
        errors.push(`Invalid: patch for ${entry.name}@${entry.version} does not ` +
        `match the patch recorded in the lock file`)
      }
    }
  }
  return errors
}

// validates that the root packageExtensions state matches what the lockfile recorded, and that the locked tree is still consistent with the rule set.
// Returns an array of human-readable error strings, empty when valid.
function validatePackageExtensions (virtualTree, idealTree) {
  const errors = []
  const lockHash = virtualTree.meta?.packageExtensionsHash || null
  const idealHash = idealTree.meta?.packageExtensionsHash || null

  if (idealHash !== lockHash) {
    if (idealHash && !lockHash) {
      errors.push('Missing: packageExtensions state from lock file')
    } else if (!idealHash && lockHash) {
      errors.push('Invalid: lock file records packageExtensions state but package.json has none')
    } else {
      errors.push('Invalid: packageExtensions in package.json do not match the lock file')
    }
    // once the canonical hashes diverge, the deeper per-node checks are moot
    return errors
  }

  // the hashes match, so validate the locked tree's own consistency against the rules
  const { PackageExtensions } = require('@npmcli/arborist')
  const root = idealTree.target || idealTree
  let pe
  try {
    pe = new PackageExtensions(root.package?.packageExtensions)
  } catch (err) {
    return [`Invalid: ${err.message}`]
  }

  for (const node of virtualTree.inventory.values()) {
    if (node.isProjectRoot || node.isWorkspace) {
      continue
    }
    // selectors match the underlying package identity, which is the alias target for aliased installs
    const name = node.packageName || node.name
    // a locked package identity must not match more than one selector
    try {
      pe.match(name, node.version)
    } catch (err) {
      errors.push(`Invalid: ${err.message}`)
    }
    // recorded provenance must still correspond to a selector that matches the node
    const applied = node.packageExtensionsApplied
    if (applied) {
      const sel = pe.selectors.find(s => s.key === applied.selector)
      if (!sel || !pe.wouldMatch(name, node.version)) {
        errors.push(
          `Invalid: stale packageExtensions provenance for ${node.name}@${node.version} (selector "${applied.selector}")`)
      }
    }
  }
  return errors
}

module.exports = validateLockfile
module.exports.validatePackageExtensions = validatePackageExtensions
