/*
 * tst.werror.js: tests basic functionality of the WError class.
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
	stacktxt = stacktxt.replace(re, 'tst.werror.js');
	return (stacktxt);
}

/*
 * Save the generic parts of all stack traces so we can avoid hardcoding
 * Node-specific implementation details in our testing of stack traces.
 */
var nodestack = new Error().stack.split('\n').slice(2).join('\n');

/* no arguments */
err = new WError();
mod_assert.equal(err.name, 'WError');
mod_assert.ok(err instanceof Error);
mod_assert.ok(err instanceof WError);
mod_assert.equal(err.message, '');
mod_assert.equal(err.toString(), 'WError');
mod_assert.ok(err.cause() === undefined);
stack = cleanStack(err.stack);
mod_assert.equal(stack, [
    'WError',
    '    at Object.<anonymous> (tst.werror.js)'
].join('\n') + '\n' + nodestack);

/* options-argument form */
err = new WError({});
mod_assert.equal(err.message, '');
mod_assert.equal(err.toString(), 'WError');
mod_assert.ok(err.cause() === undefined);

/* simple message */
err = new WError('my error');
mod_assert.equal(err.message, 'my error');
mod_assert.equal(err.toString(), 'WError: my error');
mod_assert.ok(err.cause() === undefined);
stack = cleanStack(err.stack);
mod_assert.equal(stack, [
    'WError: my error',
    '    at Object.<anonymous> (tst.werror.js)'
].join('\n') + '\n' + nodestack);

err = new WError({}, 'my error');
mod_assert.equal(err.message, 'my error');
mod_assert.equal(err.toString(), 'WError: my error');
mod_assert.ok(err.cause() === undefined);

/* printf-style message */
err = new WError('%s error: %3d problems', 'very bad', 15);
mod_assert.equal(err.message, 'very bad error:  15 problems');
mod_assert.equal(err.toString(), 'WError: very bad error:  15 problems');
mod_assert.ok(err.cause() === undefined);

err = new WError({}, '%s error: %3d problems', 'very bad', 15);
mod_assert.equal(err.message, 'very bad error:  15 problems');
mod_assert.equal(err.toString(), 'WError: very bad error:  15 problems');
mod_assert.ok(err.cause() === undefined);

/* caused by another error, with no additional message */
suberr = new Error('root cause');
err = new WError(suberr);
mod_assert.equal(err.message, '');
mod_assert.equal(err.toString(), 'WError; caused by Error: root cause');
mod_assert.ok(err.cause() === suberr);

err = new WError({ 'cause': suberr });
mod_assert.equal(err.message, '');
mod_assert.equal(err.toString(), 'WError; caused by Error: root cause');
mod_assert.ok(err.cause() === suberr);

/* caused by another error, with annotation */
err = new WError(suberr, 'proximate cause: %d issues', 3);
mod_assert.equal(err.message, 'proximate cause: 3 issues');
mod_assert.equal(err.toString(), 'WError: proximate cause: 3 issues; ' +
    'caused by Error: root cause');
mod_assert.ok(err.cause() === suberr);
stack = cleanStack(err.stack);
mod_assert.equal(stack, [
    'WError: proximate cause: 3 issues; caused by Error: root cause',
    '    at Object.<anonymous> (tst.werror.js)'
].join('\n') + '\n' + nodestack);

err = new WError({ 'cause': suberr }, 'proximate cause: %d issues', 3);
mod_assert.equal(err.message, 'proximate cause: 3 issues');
mod_assert.equal(err.toString(), 'WError: proximate cause: 3 issues; ' +
    'caused by Error: root cause');
mod_assert.ok(err.cause() === suberr);
stack = cleanStack(err.stack);
mod_assert.equal(stack, [
    'WError: proximate cause: 3 issues; caused by Error: root cause',
    '    at Object.<anonymous> (tst.werror.js)'
].join('\n') + '\n' + nodestack);

/* caused by another WError, with annotation. */
suberr = err;
err = new WError(suberr, 'top');
mod_assert.equal(err.message, 'top');
mod_assert.equal(err.toString(), 'WError: top; caused by WError: ' +
    'proximate cause: 3 issues; caused by Error: root cause');
mod_assert.ok(err.cause() === suberr);

err = new WError({ 'cause': suberr }, 'top');
mod_assert.equal(err.message, 'top');
mod_assert.equal(err.toString(), 'WError: top; caused by WError: ' +
    'proximate cause: 3 issues; caused by Error: root cause');
mod_assert.ok(err.cause() === suberr);

/* caused by a VError */
suberr = new VError(new Error('root cause'), 'mid');
err = new WError(suberr, 'top');
mod_assert.equal(err.message, 'top');
mod_assert.equal(err.toString(),
    'WError: top; caused by VError: mid: root cause');
mod_assert.ok(err.cause() === suberr);

/* null cause (for backwards compatibility with older versions) */
err = new WError(null, 'my error');
mod_assert.equal(err.message, 'my error');
mod_assert.equal(err.toString(), 'WError: my error');
mod_assert.ok(err.cause() === undefined);
stack = cleanStack(err.stack);
mod_assert.equal(stack, [
    'WError: my error',
    '    at Object.<anonymous> (tst.werror.js)'
].join('\n') + '\n' + nodestack);

err = new WError({ 'cause': null }, 'my error');
mod_assert.equal(err.message, 'my error');
mod_assert.equal(err.toString(), 'WError: my error');
mod_assert.ok(err.cause() === undefined);

err = new WError(null);
mod_assert.equal(err.message, '');
mod_assert.equal(err.toString(), 'WError');
mod_assert.ok(err.cause() === undefined);
stack = cleanStack(err.stack);
mod_assert.equal(stack, [
    'WError',
    '    at Object.<anonymous> (tst.werror.js)'
].join('\n') + '\n' + nodestack);

/* constructorOpt */
function makeErr(options) {
	return (new WError(options, 'test error'));
}
err = makeErr({});
mod_assert.equal(err.toString(), 'WError: test error');
stack = cleanStack(err.stack);
mod_assert.equal(stack, [
    'WError: test error',
    '    at makeErr (tst.werror.js)',
    '    at Object.<anonymous> (tst.werror.js)'
].join('\n') + '\n' + nodestack);

err = makeErr({ 'constructorOpt': makeErr });
mod_assert.equal(err.toString(), 'WError: test error');
stack = cleanStack(err.stack);
mod_assert.equal(stack, [
    'WError: test error',
    '    at Object.<anonymous> (tst.werror.js)'
].join('\n') + '\n' + nodestack);
