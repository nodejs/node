var Code = require('code'),
    Lab = require('lab'),
    lab = Lab.script(),
    jph = require('..'); // 'json-parse-helpfulerror'

exports.lab = lab;

lab.test('can parse', function (done) {
  var o = jph.parse('{"foo": "bar"}');

  Code.expect(o.foo).to.equal('bar');
  done();
});

lab.test('helpful error for bad JSON', function (done) {

  var bad = "{'foo': 'bar'}";

  Code.expect(function () { JSON.parse(bad) }).to.throw();

  Code.expect(function () { jph.parse(bad) }).to.throw(SyntaxError, "Unexpected token '\\'' at 1:2\n" + bad + '\n ^');

  done();
});

lab.test('fails if reviver throws', function (done) {
  function badReviver() { throw new ReferenceError('silly'); }

  Code.expect(function () { jph.parse('3', badReviver) }).to.throw(ReferenceError, 'silly');

  done();
});