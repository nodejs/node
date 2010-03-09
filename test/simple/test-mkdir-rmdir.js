require("../common");
var path = require('path');
var fs = require('fs');

var dirname = path.dirname(__filename);
var fixtures = path.join(dirname, "../fixtures");
var d = path.join(fixtures, "dir");

var mkdir_error = false;
var rmdir_error = false;

fs.mkdir(d, 0666, function (err) {
  if (err) {
    puts("mkdir error: " + err.message);
    mkdir_error = true;
  } else {
    puts("mkdir okay!");
    fs.rmdir(d, function (err) {
      if (err) {
        puts("rmdir error: " + err.message);
        rmdir_error = true;
      } else {
        puts("rmdir okay!");
      }
    });
  }
});

process.addListener("exit", function () {
  assert.equal(false, mkdir_error);
  assert.equal(false, rmdir_error);
  puts("exit");
});
