const manifestMissing = ({ tree, manifest }) => {
  // if the tree doesn't have a child by that name/version, return true
  // true means we need to install it
  const child = tree.children.get(manifest.name)
  // if no child, we have to load it
  if (!child) {
    return true
  }

  // if no version/tag specified, allow whatever's there
  if (manifest._from === `${manifest.name}@`) {
    return false
  }

  // otherwise the version has to match what we WOULD get
  return child.version !== manifest.version
}

module.exports = manifestMissing
