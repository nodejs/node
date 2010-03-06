process.mixin(require("../common"));
require("fs");
parse = require("ini").parse;

debug("load fixtures/fixture.ini");

p = path.join(fixturesDir, "fixture.ini");

fs.readFile(p,function(err, data) {
  if (err) throw err;

  assert.equal(typeof parse, 'function');

  var iniContents = parse(data);
  assert.equal(typeof iniContents, 'object');
  assert.deepEqual(iniContents,{"-":{"root":"something"},"section":{"one":"two","Foo":"Bar","this":"Your Mother!","blank":""},"Section Two":{"something else":"blah","remove":"whitespace"}})

  assert.equal(iniContents['-']['root'],'something');
  assert.equal(iniContents['section']['blank'],'');
  assert.equal(iniContents['Section Two']['remove'],'whitespace');

});
