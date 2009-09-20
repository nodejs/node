include("common.js");

var a = [116,101,115,116,32,206,163,207,131,207,128,206,177,32,226,161,140,226,160, 129,226,160,167,226,160,145];
var s = node.encodeUtf8(a);
assertEquals("test Σσπα ⡌⠁⠧⠑", s);

a = [104, 101, 108, 108, 111];
s = node.encodeUtf8(a);
assertEquals("hello", s);
