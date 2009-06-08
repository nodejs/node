var c = require("b/c.js");
exports.A = function () {
  return "A";
};
exports.C = function () {
  return c.C();
};
exports.D = function () {
  return c.D();
};
