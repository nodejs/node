module.exports = {
  importImport: x => import(x),
  requireImport: x => Promise.resolve(x).then(x => ({ default: require(x) }))
};
