module.exports = (mani, name) => {
  // skip dev because that only matters at the root,
  // where we aren't fetching a manifest from the registry
  // with multiple versions anyway.
  const {
    dependencies: deps = {},
    optionalDependencies: optDeps = {},
    peerDependencies: peerDeps = {},
  } = mani

  return deps && typeof deps[name] === 'string' ? deps[name]
    : optDeps && typeof optDeps[name] === 'string' ? optDeps[name]
    : peerDeps && typeof peerDeps[name] === 'string' ? peerDeps[name]
    : null
}
