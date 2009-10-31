var d = require("./d");

var mjsunit = require("mjsunit");

var package = require("./package");

mjsunit.assertEquals("world", package.hello);

debug("load fixtures/b/c.js");

var string = "C";

exports.C = function () {
  return string;
};

exports.D = function () {
  return d.D();
};

process.addListener("exit", function () {
  string = "C done";
  puts("b/c.js exit");
});
