foo1 = "foo1";
global.bar1 = "bar1";

var sub2 = require('./sub2');


exports.subGlobalKeys = function () {
  return sub2.globalKeys();
};

exports.subAllFooBars = function () {
  return sub2.allFooBars();
};
