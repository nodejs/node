process.stdio.writeError("\nWARNING: uri module is deprecated. Please use require(\"url\") instead.\n");

exports.decode = exports.parse = exports.format = exports.resolveObject = exports.resolve = function () {
  throw new Error("uri module is deprecated. Please use require(\"url\") instead.");
};
