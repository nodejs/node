node.blocking.print(__file__);
/*
if (node.path.dirname(__file__) !== "test-test.js") {
    throw "wrong __file__ argument";
}
*/

var mjsunit = require("./mjsunit.js");
node.blocking.print(__file__);

mjsunit.assertFalse(false, "testing the test program.");
//mjsunit.assertEquals("test-test.js", __file__);
