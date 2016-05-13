// populates missing values
module.exports = function(dst, src) {
  for (var prop in src) {
    if (src.hasOwnProperty(prop) && !dst[prop]) {
      dst[prop] = src[prop];
    }
  }
  return dst;
};
