/*
 * tst.verror.js: tests basic functionality of the VError class.
 */

var mod_assert = require('assert');
var mod_verror = require('../lib/verror');

var VError = mod_verror.VError;
var WError = mod_verror.WError;

var err, suberr, stack, substack;

/*
 * Remove full paths and relative line numbers from stack traces so that we can
 * compare against "known-good" output.
 */
function cleanStack(stacktxt)
{
	var re = new RegExp(__filename + ':\\d+:\\d+', 'gm');
	stacktxt = stacktxt.replace(re, 'tst.verror.js');
	return (stacktxt);
}

/*
 * Save the generic parts of all stack traces so we can avoid hardcoding
 * Node-specific implementation details in our testing of stack traces.
 */
var nodestack = new Error().stack.split('\n').slice(2).join('\n');

/* no arguments */
err = new VError();
mod_assert.equal(err.name, 'VError');
mod_assert.ok(err instanceof Error);
mod_assert.ok(err instanceof VError);
mod_assert.equal(err.message, '');
mod_assert.ok(err.cause() === undefined);
stack = cleanStack(err.stack);
mod_assert.equal(stack, [
    'VError',
    '    at Object.<anonymous> (tst.verror.js)'
].join('\n') + '\n' + nodestack);

/* options-argument form */
err = new VError({});
mod_assert.equal(err.message, '');
mod_assert.ok(err.cause() === undefined);

/* simple message */
err = new VError('my error');
mod_assert.equal(err.message, 'my error');
mod_assert.ok(err.cause() === undefined);
stack = cleanStack(err.stack);
mod_assert.equal(stack, [
    'VError: my error',
    '    at Object.<anonymous> (tst.verror.js)'
].join('\n') + '\n' + nodestack);

err = new VError({}, 'my error');
mod_assert.equal(err.message, 'my error');
mod_assert.ok(err.cause() === undefined);

/* printf-style message */
err = new VError('%s error: %3d problems', 'very bad', 15);
mod_assert.equal(err.message, 'very bad error:  15 problems');
mod_assert.ok(err.cause() === undefined);

err = new VError({}, '%s error: %3d problems', 'very bad', 15);
mod_assert.equal(err.message, 'very bad error:  15 problems');
mod_assert.ok(err.cause() === undefined);

/* caused by another error, with no additional message */
suberr = new Error('root cause');
err = new VError(suberr);
mod_assert.equal(err.message, ': root cause');
mod_assert.ok(err.cause() === suberr);

err = new VError({ 'cause': suberr });
mod_assert.equal(err.message, ': root cause');
mod_assert.ok(err.cause() === suberr);

/* caused by another error, with annotation */
err = new VError(suberr, 'proximate cause: %d issues', 3);
mod_assert.equal(err.message, 'proximate cause: 3 issues: root cause');
mod_assert.ok(err.cause() === suberr);
stack = cleanStack(err.stack);
mod_assert.equal(stack, [
    'VError: proximate cause: 3 issues: root cause',
    '    at Object.<anonymous> (tst.verror.js)'
].join('\n') + '\n' + nodestack);

err = new VError({ 'cause': suberr }, 'proximate cause: %d issues', 3);
mod_assert.equal(err.message, 'proximate cause: 3 issues: root cause');
mod_assert.ok(err.cause() === suberr);
stack = cleanStack(err.stack);
mod_assert.equal(stack, [
    'VError: proximate cause: 3 issues: root cause',
    '    at Object.<anonymous> (tst.verror.js)'
].join('\n') + '\n' + nodestack);

/* caused by another VError, with annotation. */
suberr = err;
err = new VError(suberr, 'top');
mod_assert.equal(err.message, 'top: proximate cause: 3 issues: root cause');
mod_assert.ok(err.cause() === suberr);

err = new VError({ 'cause': suberr }, 'top');
mod_assert.equal(err.message, 'top: proximate cause: 3 issues: root cause');
mod_assert.ok(err.cause() === suberr);

/* caused by a WError */
suberr = new WError(new Error('root cause'), 'mid');
err = new VError(suberr, 'top');
mod_assert.equal(err.message, 'top: mid');
mod_assert.ok(err.cause() === suberr);

/* null cause (for backwards compatibility with older versions) */
err = new VError(null, 'my error');
mod_assert.equal(err.message, 'my error');
mod_assert.ok(err.cause() === undefined);
stack = cleanStack(err.stack);
mod_assert.equal(stack, [
    'VError: my error',
    '    at Object.<anonymous> (tst.verror.js)'
].join('\n') + '\n' + nodestack);

err = new VError({ 'cause': null }, 'my error');
mod_assert.equal(err.message, 'my error');
mod_assert.ok(err.cause() === undefined);

err = new VError(null);
mod_assert.equal(err.message, '');
mod_assert.ok(err.cause() === undefined);
stack = cleanStack(err.stack);
mod_assert.equal(stack, [
    'VError',
    '    at Object.<anonymous> (tst.verror.js)'
].join('\n') + '\n' + nodestack);

/* constructorOpt */
function makeErr(options) {
	return (new VError(options, 'test error'));
}
err = makeErr({});
stack = cleanStack(err.stack);
mod_assert.equal(stack, [
    'VError: test error',
    '    at makeErr (tst.verror.js)',
    '    at Object.<anonymous> (tst.verror.js)'
].join('\n') + '\n' + nodestack);

err = makeErr({ 'constructorOpt': makeErr });
stack = cleanStack(err.stack);
mod_assert.equal(stack, [
    'VError: test error',
    '    at Object.<anonymous> (tst.verror.js)'
].join('\n') + '\n' + nodestack);
