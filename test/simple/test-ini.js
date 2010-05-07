require("../common");
var path = require('path'),
  fs = require("fs"),
  ini = require("ini"),
  parse = require("ini").parse;

debug("load fixtures/fixture.ini");

p = path.join(fixturesDir, "fixture.ini");

fs.readFile(p, 'utf8', function(err, data) {
  if (err) throw err;

  assert.equal(typeof parse, 'function');

  var iniContents = parse(data);
  assert.equal(typeof iniContents, 'object');
  
  var expect = { "-" :
                 { "root" : "something"
                 , "url" : "http://example.com/?foo=bar"
                 }
               , "the section with whitespace" :
                 { "this has whitespace" : "yep"
                 , "just a flag, no value." : true
                 }
               , "section" :
                 { "one" : "two"
                 , "Foo" : "Bar"
                 , "this" : "Your Mother!"
                 , "blank" : ""
                 }
               , "Section Two" :
                 { "something else" : "blah"
                 , "remove" : "whitespace"
                 }
               },
    expectStr = "root = something\n"+
                "url = http://example.com/?foo=bar\n"+
                "[the section with whitespace]\n"+
                "this has whitespace = yep\n"+
                "just a flag, no value.\n"+
                "[section]\n"+
                "one = two\n"+
                "Foo = Bar\n"+
                "this = Your Mother!\n"+
                "blank = \n"+
                "[Section Two]\n"+
                "something else = blah\n"+
                "remove = whitespace\n";

  assert.deepEqual(iniContents, expect,
    "actual: \n"+inspect(iniContents) +"\n≠\nexpected:\n"+inspect(expect));

  assert.equal(ini.stringify(iniContents), expectStr,
    "actual: \n"+inspect(ini.stringify(iniContents)) +"\n≠\nexpected:\n"+inspect(expectStr));
});


