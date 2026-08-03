exports.cjs = function(key) {
  return require(key);
};
exports.esm = function(key) {
  return import(key);
};
