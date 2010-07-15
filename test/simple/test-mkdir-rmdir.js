common = require("../common");
assert = common.assert
var path = require('path');
var fs = require('fs');

var dirname = path.dirname(__filename);
var fixtures = path.join(dirname, "../fixtures");
var d = path.join(fixtures, "dir");

var mkdir_error = false;
var rmdir_error = false;

fs.mkdir(d, 0666, function (err) {
  if (err) {
    console.log("mkdir error: " + err.message);
    mkdir_error = true;
  } else {
    console.log("mkdir okay!");
    fs.rmdir(d, function (err) {
      if (err) {
        console.log("rmdir error: " + err.message);
        rmdir_error = true;
      } else {
        console.log("rmdir okay!");
      }
    });
  }
});

process.addListener("exit", function () {
  assert.equal(false, mkdir_error);
  assert.equal(false, rmdir_error);
  console.log("exit");
});
