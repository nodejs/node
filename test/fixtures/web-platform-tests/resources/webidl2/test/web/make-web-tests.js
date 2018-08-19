
// generates tests that work in a browser

// XXX
//  have it run through valid and invalid properly

var pth = require("path")
,   fs = require("fs")
,   dir = function (path) {
        return pth.join(__dirname, "..", path);
    }
,   allFromDir = function (dir, ext, asJSON) {
        return fs.readdirSync(dir)
                 .filter(function (it) { return ext.test(it); })
                 .map(function (it) {
                     var cnt = fs.readFileSync(pth.join(dir, it), "utf8");
                     return asJSON ? JSON.parse(cnt) : cnt;
                 });
    }
,   data = {
        valid:  {
            json:   allFromDir(dir("syntax/json"), /\.json$/, true)
        ,   idl:    allFromDir(dir("syntax/idl"), /\.w?idl$/, false)
        }
    ,   invalid:{
            json:   allFromDir(dir("invalid/json"), /\.json$/, true)
        ,   idl:    allFromDir(dir("invalid/idl"), /\.w?idl$/, false)
        }
    }
,   html = [
        "<!DOCTYPE html>"
    ,   "<html>"
    ,   "  <head><meta charset='utf-8''><link rel='stylesheet' href='../../node_modules/mocha/mocha.css'>"
    ,   "    <title>WebIDL2 Browser Tests</title>"
    ,   "    <script>var data = " + JSON.stringify(data) + "</script>"
    ,   "  </head>"
    ,   "  <body><div id='mocha'></div>"
    ,   "  <script src='https://ajax.googleapis.com/ajax/libs/jquery/1.8.3/jquery.min.js'></script>"
    ,   "  <script src='../../node_modules/expect/umd/expect.min.js'></script>"
    ,   "  <script src='../../node_modules/expect.js/index.js'></script>"
    ,   "  <script src='../../node_modules/mocha/mocha.js'></script>"
    ,   "  <script src='../../node_modules/jsondiffpatch/public/build/jsondiffpatch.min.js'></script>"
    ,   "  <script src='../../lib/webidl2.js'></script>"
    ,   "  <script>mocha.setup('bdd');</script>"
    ,   "  <script src='run-tests.js'></script>"
    ,   "  <script>mocha.run();</script>"
    ,   "  </body>"
    ,   "</html>"
    ].join("\n")
;

fs.writeFileSync("browser-tests.html", html, "utf8");
