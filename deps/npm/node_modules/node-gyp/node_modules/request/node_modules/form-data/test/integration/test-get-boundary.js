var common = require('../common');
var assert = common.assert;

var FormData = require(common.dir.lib + '/form_data');

(function testOneBoundaryPerForm() {
  var form = new FormData();
  var boundary = form.getBoundary();

  assert.equal(boundary, form.getBoundary());
  assert.equal(boundary.length, 50);
})();

(function testUniqueBoundaryPerForm() {
  var formA = new FormData();
  var formB = new FormData();
  assert.notEqual(formA.getBoundary(), formB.getBoundary());
})();
