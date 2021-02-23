module.exports = (mani, name) => {
  // skip dev because that only matters at the root,
  // where we aren't fetching a manifest from the registry
  // with multiple versions anyway.
  const {
    dependencies: deps = {},
    optionalDependencies: optDeps = {},
    peerDependencies: peerDeps = {},
  } = mani

  return typeof deps[name] === 'string' ? deps[name]
    : typeof optDeps[name] === 'string' ? optDeps[name]
    : typeof peerDeps[name] === 'string' ? peerDeps[name]
    : null
}
