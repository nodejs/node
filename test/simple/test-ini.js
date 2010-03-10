require("../common");
var path = require('path');
var fs = require("fs");
parse = require("ini").parse;

debug("load fixtures/fixture.ini");

p = path.join(fixturesDir, "fixture.ini");

fs.readFile(p,function(err, data) {
  if (err) throw err;

  assert.equal(typeof parse, 'function');

  var iniContents = parse(data);
  assert.equal(typeof iniContents, 'object');
  
  var expect =
    { "-" :
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
    };
  
  assert.deepEqual(iniContents, expect,
    "actual: \n"+inspect(iniContents) +"\n≠\nexpected:\n"+inspect(expect))

  assert.equal(iniContents['-']['root'],'something');
  assert.equal(iniContents['section']['blank'],'');
  assert.equal(iniContents['Section Two']['remove'],'whitespace');

});
