global.expect = require('chai').expect;
var has = require('../src');


describe('has', function() {
  it('works!', function() {
    expect(has({}, 'hasOwnProperty')).to.be.false;
    expect(has(Object.prototype, 'hasOwnProperty')).to.be.true;
  });
});
