var c = require("b/c.js");
var string = "A";

exports.A = function () {
  return string;
};

exports.C = function () {
  return c.C();
};

exports.D = function () {
  return c.D();
};

function onExit () {
  string = "A done";
}
