module.exports = (mani, name) => {
  // skip dev because that only matters at the root,
  // where we aren't fetching a manifest from the registry
  // with multiple versions anyway.
  return mani.dependencies && typeof mani.dependencies[name] === 'string'
    ? mani.dependencies[name]
    : mani.optionalDependencies && typeof mani.optionalDependencies[name] === 'string'
    ? mani.optionalDependencies[name]
    : mani.peerDependencies && typeof mani.peerDependencies[name] === 'string'
    ? mani.peerDependencies[name]
    : null
}

