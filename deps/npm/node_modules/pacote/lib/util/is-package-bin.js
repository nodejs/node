// Function to determine whether a path is in the package.bin set.
// Used to prevent issues when people publish a package from a
// windows machine, and then install with --no-bin-links.
//
// Note: this is not possible in remote or file fetchers, since
// we don't have the manifest until AFTER we've unpacked.  But the
// main use case is registry fetching with git a distant second,
// so that's an acceptable edge case to not handle.

const binObj = (name, bin) =>
  typeof bin === 'string' ? { [name]: bin } : bin

const hasBin = (pkg, path) => {
  const bin = binObj(pkg.name, pkg.bin)
  const p = path.replace(/^[^\\\/]*\//, '')
  for (const [k, v] of Object.entries(bin)) {
    if (v === p)
      return true
  }
  return false
}

module.exports = (pkg, path) =>
  pkg && pkg.bin ? hasBin(pkg, path) : false
