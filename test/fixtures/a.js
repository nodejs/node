var c = require("./b/c");

debug("load fixtures/a.js");

var string = "A";

exports.SomeClass = c.SomeClass;

exports.A = function () {
  return string;
};

exports.C = function () {
  return c.C();
};

exports.D = function () {
  return c.D();
};

process.addListener("exit", function () {
  string = "A done";
});
