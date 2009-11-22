process.mixin(require("./common"));

var mask = 0664;
var old = process.umask(mask);

assertEquals(true, mask === process.umask(old));
