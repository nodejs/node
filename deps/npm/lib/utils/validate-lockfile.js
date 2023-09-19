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
  }
  return errors
}

module.exports = validateLockfile
