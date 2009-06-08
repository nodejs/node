var d = require("d.js");

var string = "C";

exports.C = function () {
  return string;
};

exports.D = function () {
  return d.D();
};

function onExit () {
  string = "C done";
}
