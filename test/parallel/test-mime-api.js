// Flags: --expose-internals
'use strict';

require('../common');
const assert = require('assert');
const { MIMEType, MIMEParams } = require('util');

const WHITESPACES = '\t\n\f\r ';
const NOT_HTTP_TOKEN_CODE_POINT = ',';
const NOT_HTTP_QUOTED_STRING_CODE_POINT = '\n';

const mime = new MIMEType('application/ecmascript; ');
const mime_descriptors = Object.getOwnPropertyDescriptors(mime);
const mime_proto = Object.getPrototypeOf(mime);
const mime_impersonator = { __proto__: mime_proto };
for (const key of Object.keys(mime_descriptors)) {
  const descriptor = mime_descriptors[key];
  if (descriptor.get) {
    assert.throws(descriptor.get.call(mime_impersonator), /Invalid receiver/i);
  }
  if (descriptor.set) {
    assert.throws(descriptor.set.call(mime_impersonator, 'x'), /Invalid receiver/i);
  }
}


assert.strictEqual(
  JSON.stringify(mime),
  JSON.stringify('application/ecmascript'));
assert.strictEqual(`${mime}`, 'application/ecmascript');
assert.strictEqual(mime.essence, 'application/ecmascript');
assert.strictEqual(mime.type, 'application');
assert.strictEqual(mime.subtype, 'ecmascript');
assert.ok(mime.params);
assert.deepStrictEqual([], [...mime.params]);
assert.strictEqual(mime.params.has('not found'), false);
assert.strictEqual(mime.params.get('not found'), null);
assert.strictEqual(mime.params.delete('not found'), undefined);


mime.type = 'text';
assert.strictEqual(mime.type, 'text');
assert.strictEqual(JSON.stringify(mime), JSON.stringify('text/ecmascript'));
assert.strictEqual(`${mime}`, 'text/ecmascript');
assert.strictEqual(mime.essence, 'text/ecmascript');

assert.throws(() => {
  mime.type = `${WHITESPACES}text`;
}, /ERR_INVALID_MIME_SYNTAX/);

assert.throws(() => mime.type = '', /type/i);
assert.throws(() => mime.type = '/', /type/i);
assert.throws(() => mime.type = 'x/', /type/i);
assert.throws(() => mime.type = '/x', /type/i);
assert.throws(() => mime.type = NOT_HTTP_TOKEN_CODE_POINT, /type/i);
assert.throws(() => mime.type = `${NOT_HTTP_TOKEN_CODE_POINT}/`, /type/i);
assert.throws(() => mime.type = `/${NOT_HTTP_TOKEN_CODE_POINT}`, /type/i);


mime.subtype = 'javascript';
assert.strictEqual(mime.type, 'text');
assert.strictEqual(JSON.stringify(mime), JSON.stringify('text/javascript'));
assert.strictEqual(`${mime}`, 'text/javascript');
assert.strictEqual(mime.essence, 'text/javascript');
assert.strictEqual(`${mime.params}`, '');
assert.strictEqual(`${new MIMEParams()}`, '');
assert.strictEqual(`${new MIMEParams(mime.params)}`, '');
assert.strictEqual(`${new MIMEParams(`${mime.params}`)}`, '');

assert.throws(() => {
  mime.subtype = `javascript${WHITESPACES}`;
}, /ERR_INVALID_MIME_SYNTAX/);

assert.throws(() => mime.subtype = '', /subtype/i);
assert.throws(() => mime.subtype = ';', /subtype/i);
assert.throws(() => mime.subtype = 'x;', /subtype/i);
assert.throws(() => mime.subtype = ';x', /subtype/i);
assert.throws(() => mime.subtype = NOT_HTTP_TOKEN_CODE_POINT, /subtype/i);
assert.throws(
  () => mime.subtype = `${NOT_HTTP_TOKEN_CODE_POINT};`,
  /subtype/i);
assert.throws(
  () => mime.subtype = `;${NOT_HTTP_TOKEN_CODE_POINT}`,
  /subtype/i);


const params = mime.params;
params.set('charset', 'utf-8');
assert.strictEqual(params.has('charset'), true);
assert.strictEqual(params.get('charset'), 'utf-8');
assert.deepStrictEqual([...params], [['charset', 'utf-8']]);
assert.strictEqual(
  JSON.stringify(mime),
  JSON.stringify('text/javascript;charset=utf-8'));
assert.strictEqual(`${mime}`, 'text/javascript;charset=utf-8');
assert.strictEqual(mime.essence, 'text/javascript');
assert.strictEqual(`${mime.params}`, 'charset=utf-8');
assert.strictEqual(`${new MIMEParams(mime.params)}`, '');
assert.strictEqual(`${new MIMEParams(`${mime.params}`)}`, '');

params.set('goal', 'module');
assert.strictEqual(params.has('goal'), true);
assert.strictEqual(params.get('goal'), 'module');
assert.deepStrictEqual([...params], [['charset', 'utf-8'], ['goal', 'module']]);
assert.strictEqual(
  JSON.stringify(mime),
  JSON.stringify('text/javascript;charset=utf-8;goal=module'));
assert.strictEqual(`${mime}`, 'text/javascript;charset=utf-8;goal=module');
assert.strictEqual(mime.essence, 'text/javascript');
assert.strictEqual(`${mime.params}`, 'charset=utf-8;goal=module');
assert.strictEqual(`${new MIMEParams(mime.params)}`, '');
assert.strictEqual(`${new MIMEParams(`${mime.params}`)}`, '');

assert.throws(() => {
  params.set(`${WHITESPACES}goal`, 'module');
}, /ERR_INVALID_MIME_SYNTAX/);

params.set('charset', 'iso-8859-1');
assert.strictEqual(params.has('charset'), true);
assert.strictEqual(params.get('charset'), 'iso-8859-1');
assert.deepStrictEqual(
  [...params],
  [['charset', 'iso-8859-1'], ['goal', 'module']]);
assert.strictEqual(
  JSON.stringify(mime),
  JSON.stringify('text/javascript;charset=iso-8859-1;goal=module'));
assert.strictEqual(`${mime}`, 'text/javascript;charset=iso-8859-1;goal=module');
assert.strictEqual(mime.essence, 'text/javascript');

params.delete('charset');
assert.strictEqual(params.has('charset'), false);
assert.strictEqual(params.get('charset'), null);
assert.deepStrictEqual([...params], [['goal', 'module']]);
assert.strictEqual(
  JSON.stringify(mime),
  JSON.stringify('text/javascript;goal=module'));
assert.strictEqual(`${mime}`, 'text/javascript;goal=module');
assert.strictEqual(mime.essence, 'text/javascript');

params.set('x', '');
assert.strictEqual(params.has('x'), true);
assert.strictEqual(params.get('x'), '');
assert.deepStrictEqual([...params], [['goal', 'module'], ['x', '']]);
assert.strictEqual(
  JSON.stringify(mime),
  JSON.stringify('text/javascript;goal=module;x=""'));
assert.strictEqual(`${mime}`, 'text/javascript;goal=module;x=""');
assert.strictEqual(mime.essence, 'text/javascript');

assert.throws(() => params.set('', 'x'), /parameter name/i);
assert.throws(() => params.set('=', 'x'), /parameter name/i);
assert.throws(() => params.set('x=', 'x'), /parameter name/i);
assert.throws(() => params.set('=x', 'x'), /parameter name/i);
assert.throws(() => params.set(`${NOT_HTTP_TOKEN_CODE_POINT}=`, 'x'), /parameter name/i);
assert.throws(() => params.set(`${NOT_HTTP_TOKEN_CODE_POINT}x`, 'x'), /parameter name/i);
assert.throws(() => params.set(`x${NOT_HTTP_TOKEN_CODE_POINT}`, 'x'), /parameter name/i);

assert.throws(() => params.set('x', `${NOT_HTTP_QUOTED_STRING_CODE_POINT};`), /parameter value/i);
assert.throws(() => params.set('x', `${NOT_HTTP_QUOTED_STRING_CODE_POINT}x`), /parameter value/i);
assert.throws(() => params.set('x', `x${NOT_HTTP_QUOTED_STRING_CODE_POINT}`), /parameter value/i);
