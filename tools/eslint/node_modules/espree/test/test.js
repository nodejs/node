/**
 * @fileoverview Tests for parsing/tokenization.
 * @author Nicholas C. Zakas
 * @copyright 2014 Nicholas C. Zakas. All rights reserved.
 */

//------------------------------------------------------------------------------
// Requirements
//------------------------------------------------------------------------------

var shelljs = require("shelljs"),
    fs = require("fs"),
    path = require("path");

//------------------------------------------------------------------------------
// Processing
//------------------------------------------------------------------------------

var files = shelljs.find("./tests/fixtures/ast");

files.filter(function(filename) {
    return path.extname(filename) === ".json";
}).forEach(function(filename) {
    var basename = path.basename(filename, ".json");
    exports[basename] = JSON.parse(fs.readFileSync(filename, "utf8"), function(key, value) {

        // JSON can't represent undefined, so we use a special value
        if (value === "espree@undefined") {
            return undefined;
        } else {
            return value;
        }
    });
});
