/*
 * tst.inherit.js: test that inheriting from VError and WError work as expected.
 */

var mod_assert = require('assert');
var mod_util = require('util');

var mod_verror = require('../lib/verror');

var VError = mod_verror.VError;
var WError = mod_verror.WError;
var err, suberr;

function VErrorChild()
{
	VError.apply(this, Array.prototype.slice.call(arguments));
}

mod_util.inherits(VErrorChild, VError);
VErrorChild.prototype.name = 'VErrorChild';


function WErrorChild()
{
	WError.apply(this, Array.prototype.slice.call(arguments));
}

mod_util.inherits(WErrorChild, WError);
WErrorChild.prototype.name = 'WErrorChild';


suberr = new Error('root cause');
err = new VErrorChild(suberr, 'top');
mod_assert.ok(err instanceof Error);
mod_assert.ok(err instanceof VError);
mod_assert.ok(err instanceof VErrorChild);
mod_assert.equal(err.cause(), suberr);
mod_assert.equal(err.message, 'top: root cause');
mod_assert.equal(err.toString(), 'VErrorChild: top: root cause');
mod_assert.equal(err.stack.split('\n')[0], 'VErrorChild: top: root cause');

suberr = new Error('root cause');
err = new WErrorChild(suberr, 'top');
mod_assert.ok(err instanceof Error);
mod_assert.ok(err instanceof WError);
mod_assert.ok(err instanceof WErrorChild);
mod_assert.equal(err.cause(), suberr);
mod_assert.equal(err.message, 'top');
mod_assert.equal(err.toString(),
	'WErrorChild: top; caused by Error: root cause');
mod_assert.equal(err.stack.split('\n')[0],
	'WErrorChild: top; caused by Error: root cause');


// Test that `<Ctor>.toString()` uses the ctor name. I.e. setting
// `<Ctor>.prototype.name` isn't necessary.
function VErrorChildNoName() {
	VError.apply(this, Array.prototype.slice.call(arguments));
}
mod_util.inherits(VErrorChildNoName, VError);
err = new VErrorChildNoName('top');
mod_assert.equal(err.toString(), 'VErrorChildNoName: top');

function WErrorChildNoName() {
	WError.apply(this, Array.prototype.slice.call(arguments));
}
mod_util.inherits(WErrorChildNoName, WError);
err = new WErrorChildNoName('top');
mod_assert.equal(err.toString(), 'WErrorChildNoName: top');


// Test that `<Ctor>.prototype.name` can be used for the `.toString()`
// when the ctor is anonymous.
var VErrorChildAnon = function () {
	VError.apply(this, Array.prototype.slice.call(arguments));
};
mod_util.inherits(VErrorChildAnon, VError);
VErrorChildAnon.prototype.name = 'VErrorChildAnon';
err = new VErrorChildAnon('top');
mod_assert.equal(err.toString(), 'VErrorChildAnon: top');

var WErrorChildAnon = function () {
	WError.apply(this, Array.prototype.slice.call(arguments));
};
mod_util.inherits(WErrorChildAnon, WError);
WErrorChildAnon.prototype.name = 'WErrorChildAnon';
err = new WErrorChildAnon('top');
mod_assert.equal(err.toString(), 'WErrorChildAnon: top');


// Test get appropriate exception name in `.toString()` when reconstituting
// an error instance a la:
//    https://github.com/mcavage/node-fast/blob/master/lib/client.js#L215
err = new VError('top');
err.name = 'CustomNameError';
mod_assert.equal(err.toString(), 'CustomNameError: top');

err = new WError('top');
err.name = 'CustomNameError';
mod_assert.equal(err.toString(), 'CustomNameError: top');
