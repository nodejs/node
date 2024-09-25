/* This file is mostly a remix of @zcorpan’s web worker test suite */

structuredCloneBatteryOfTests = [];

function check(description, input, callback, requiresDocument = false) {
  structuredCloneBatteryOfTests.push({
    description,
    async f(runner) {
      let newInput = input;
      if (typeof input === 'function') {
        newInput = input();
      }
      const copy = await runner.structuredClone(newInput);
      await callback(copy, newInput);
    },
    requiresDocument
  });
}

function compare_primitive(actual, input) {
  assert_equals(actual, input);
}
function compare_Array(callback) {
  return async function(actual, input) {
    if (typeof actual === 'string')
      assert_unreached(actual);
    assert_true(actual instanceof Array, 'instanceof Array');
    assert_not_equals(actual, input);
    assert_equals(actual.length, input.length, 'length');
    await callback(actual, input);
  }
}

function compare_Object(callback) {
  return async function(actual, input) {
    if (typeof actual === 'string')
      assert_unreached(actual);
    assert_true(actual instanceof Object, 'instanceof Object');
    assert_false(actual instanceof Array, 'instanceof Array');
    assert_not_equals(actual, input);
    await callback(actual, input);
  }
}

function enumerate_props(compare_func) {
  return async function(actual, input) {
    for (const x in input) {
      await compare_func(actual[x], input[x]);
    }
  };
}

check('primitive undefined', undefined, compare_primitive);
check('primitive null', null, compare_primitive);
check('primitive true', true, compare_primitive);
check('primitive false', false, compare_primitive);
check('primitive string, empty string', '', compare_primitive);
check('primitive string, lone high surrogate', '\uD800', compare_primitive);
check('primitive string, lone low surrogate', '\uDC00', compare_primitive);
check('primitive string, NUL', '\u0000', compare_primitive);
check('primitive string, astral character', '\uDBFF\uDFFD', compare_primitive);
check('primitive number, 0.2', 0.2, compare_primitive);
check('primitive number, 0', 0, compare_primitive);
check('primitive number, -0', -0, compare_primitive);
check('primitive number, NaN', NaN, compare_primitive);
check('primitive number, Infinity', Infinity, compare_primitive);
check('primitive number, -Infinity', -Infinity, compare_primitive);
check('primitive number, 9007199254740992', 9007199254740992, compare_primitive);
check('primitive number, -9007199254740992', -9007199254740992, compare_primitive);
check('primitive number, 9007199254740994', 9007199254740994, compare_primitive);
check('primitive number, -9007199254740994', -9007199254740994, compare_primitive);
check('primitive BigInt, 0n', 0n, compare_primitive);
check('primitive BigInt, -0n', -0n, compare_primitive);
check('primitive BigInt, -9007199254740994000n', -9007199254740994000n, compare_primitive);
check('primitive BigInt, -9007199254740994000900719925474099400090071992547409940009007199254740994000n', -9007199254740994000900719925474099400090071992547409940009007199254740994000n, compare_primitive);

check('Array primitives', [undefined,
                           null,
                           true,
                           false,
                           '',
                           '\uD800',
                           '\uDC00',
                           '\u0000',
                           '\uDBFF\uDFFD',
                           0.2,
                           0,
                           -0,
                           NaN,
                           Infinity,
                           -Infinity,
                           9007199254740992,
                           -9007199254740992,
                           9007199254740994,
                           -9007199254740994,
                           -12n,
                           -0n,
                           0n], compare_Array(enumerate_props(compare_primitive)));
check('Object primitives', {'undefined':undefined,
                           'null':null,
                           'true':true,
                           'false':false,
                           'empty':'',
                           'high surrogate':'\uD800',
                           'low surrogate':'\uDC00',
                           'nul':'\u0000',
                           'astral':'\uDBFF\uDFFD',
                           '0.2':0.2,
                           '0':0,
                           '-0':-0,
                           'NaN':NaN,
                           'Infinity':Infinity,
                           '-Infinity':-Infinity,
                           '9007199254740992':9007199254740992,
                           '-9007199254740992':-9007199254740992,
                           '9007199254740994':9007199254740994,
                           '-9007199254740994':-9007199254740994}, compare_Object(enumerate_props(compare_primitive)));

function compare_Boolean(actual, input) {
  if (typeof actual === 'string')
    assert_unreached(actual);
  assert_true(actual instanceof Boolean, 'instanceof Boolean');
  assert_equals(String(actual), String(input), 'converted to primitive');
  assert_not_equals(actual, input);
}
check('Boolean true', new Boolean(true), compare_Boolean);
check('Boolean false', new Boolean(false), compare_Boolean);
check('Array Boolean objects', [new Boolean(true), new Boolean(false)], compare_Array(enumerate_props(compare_Boolean)));
check('Object Boolean objects', {'true':new Boolean(true), 'false':new Boolean(false)}, compare_Object(enumerate_props(compare_Boolean)));

function compare_obj(what) {
  const Type = self[what];
  return function(actual, input) {
    if (typeof actual === 'string')
      assert_unreached(actual);
    assert_true(actual instanceof Type, 'instanceof '+what);
    assert_equals(Type(actual), Type(input), 'converted to primitive');
    assert_not_equals(actual, input);
  };
}
check('String empty string', new String(''), compare_obj('String'));
check('String lone high surrogate', new String('\uD800'), compare_obj('String'));
check('String lone low surrogate', new String('\uDC00'), compare_obj('String'));
check('String NUL', new String('\u0000'), compare_obj('String'));
check('String astral character', new String('\uDBFF\uDFFD'), compare_obj('String'));
check('Array String objects', [new String(''),
                               new String('\uD800'),
                               new String('\uDC00'),
                               new String('\u0000'),
                               new String('\uDBFF\uDFFD')], compare_Array(enumerate_props(compare_obj('String'))));
check('Object String objects', {'empty':new String(''),
                               'high surrogate':new String('\uD800'),
                               'low surrogate':new String('\uDC00'),
                               'nul':new String('\u0000'),
                               'astral':new String('\uDBFF\uDFFD')}, compare_Object(enumerate_props(compare_obj('String'))));

check('Number 0.2', new Number(0.2), compare_obj('Number'));
check('Number 0', new Number(0), compare_obj('Number'));
check('Number -0', new Number(-0), compare_obj('Number'));
check('Number NaN', new Number(NaN), compare_obj('Number'));
check('Number Infinity', new Number(Infinity), compare_obj('Number'));
check('Number -Infinity', new Number(-Infinity), compare_obj('Number'));
check('Number 9007199254740992', new Number(9007199254740992), compare_obj('Number'));
check('Number -9007199254740992', new Number(-9007199254740992), compare_obj('Number'));
check('Number 9007199254740994', new Number(9007199254740994), compare_obj('Number'));
check('Number -9007199254740994', new Number(-9007199254740994), compare_obj('Number'));
// BigInt does not have a non-throwing constructor
check('BigInt -9007199254740994n', Object(-9007199254740994n), compare_obj('BigInt'));

check('Array Number objects', [new Number(0.2),
                               new Number(0),
                               new Number(-0),
                               new Number(NaN),
                               new Number(Infinity),
                               new Number(-Infinity),
                               new Number(9007199254740992),
                               new Number(-9007199254740992),
                               new Number(9007199254740994),
                               new Number(-9007199254740994)], compare_Array(enumerate_props(compare_obj('Number'))));
check('Object Number objects', {'0.2':new Number(0.2),
                               '0':new Number(0),
                               '-0':new Number(-0),
                               'NaN':new Number(NaN),
                               'Infinity':new Number(Infinity),
                               '-Infinity':new Number(-Infinity),
                               '9007199254740992':new Number(9007199254740992),
                               '-9007199254740992':new Number(-9007199254740992),
                               '9007199254740994':new Number(9007199254740994),
                               '-9007199254740994':new Number(-9007199254740994)}, compare_Object(enumerate_props(compare_obj('Number'))));

function compare_Date(actual, input) {
  if (typeof actual === 'string')
    assert_unreached(actual);
  assert_true(actual instanceof Date, 'instanceof Date');
  assert_equals(Number(actual), Number(input), 'converted to primitive');
  assert_not_equals(actual, input);
}
check('Date 0', new Date(0), compare_Date);
check('Date -0', new Date(-0), compare_Date);
check('Date -8.64e15', new Date(-8.64e15), compare_Date);
check('Date 8.64e15', new Date(8.64e15), compare_Date);
check('Array Date objects', [new Date(0),
                             new Date(-0),
                             new Date(-8.64e15),
                             new Date(8.64e15)], compare_Array(enumerate_props(compare_Date)));
check('Object Date objects', {'0':new Date(0),
                              '-0':new Date(-0),
                              '-8.64e15':new Date(-8.64e15),
                              '8.64e15':new Date(8.64e15)}, compare_Object(enumerate_props(compare_Date)));

function compare_RegExp(expected_source) {
  // XXX ES6 spec doesn't define exact serialization for `source` (it allows several ways to escape)
  return function(actual, input) {
    if (typeof actual === 'string')
      assert_unreached(actual);
    assert_true(actual instanceof RegExp, 'instanceof RegExp');
    assert_equals(actual.global, input.global, 'global');
    assert_equals(actual.ignoreCase, input.ignoreCase, 'ignoreCase');
    assert_equals(actual.multiline, input.multiline, 'multiline');
    assert_equals(actual.source, expected_source, 'source');
    assert_equals(actual.sticky, input.sticky, 'sticky');
    assert_equals(actual.unicode, input.unicode, 'unicode');
    assert_equals(actual.lastIndex, 0, 'lastIndex');
    assert_not_equals(actual, input);
  }
}
function func_RegExp_flags_lastIndex() {
  const r = /foo/gim;
  r.lastIndex = 2;
  return r;
}
function func_RegExp_sticky() {
  return new RegExp('foo', 'y');
}
function func_RegExp_unicode() {
  return new RegExp('foo', 'u');
}
check('RegExp flags and lastIndex', func_RegExp_flags_lastIndex, compare_RegExp('foo'));
check('RegExp sticky flag', func_RegExp_sticky, compare_RegExp('foo'));
check('RegExp unicode flag', func_RegExp_unicode, compare_RegExp('foo'));
check('RegExp empty', new RegExp(''), compare_RegExp('(?:)'));
check('RegExp slash', new RegExp('/'), compare_RegExp('\\/'));
check('RegExp new line', new RegExp('\n'), compare_RegExp('\\n'));
check('Array RegExp object, RegExp flags and lastIndex', [func_RegExp_flags_lastIndex()], compare_Array(enumerate_props(compare_RegExp('foo'))));
check('Array RegExp object, RegExp sticky flag', function() { return [func_RegExp_sticky()]; }, compare_Array(enumerate_props(compare_RegExp('foo'))));
check('Array RegExp object, RegExp unicode flag', function() { return [func_RegExp_unicode()]; }, compare_Array(enumerate_props(compare_RegExp('foo'))));
check('Array RegExp object, RegExp empty', [new RegExp('')], compare_Array(enumerate_props(compare_RegExp('(?:)'))));
check('Array RegExp object, RegExp slash', [new RegExp('/')], compare_Array(enumerate_props(compare_RegExp('\\/'))));
check('Array RegExp object, RegExp new line', [new RegExp('\n')], compare_Array(enumerate_props(compare_RegExp('\\n'))));
check('Object RegExp object, RegExp flags and lastIndex', {'x':func_RegExp_flags_lastIndex()}, compare_Object(enumerate_props(compare_RegExp('foo'))));
check('Object RegExp object, RegExp sticky flag', function() { return {'x':func_RegExp_sticky()}; }, compare_Object(enumerate_props(compare_RegExp('foo'))));
check('Object RegExp object, RegExp unicode flag', function() { return {'x':func_RegExp_unicode()}; }, compare_Object(enumerate_props(compare_RegExp('foo'))));
check('Object RegExp object, RegExp empty', {'x':new RegExp('')}, compare_Object(enumerate_props(compare_RegExp('(?:)'))));
check('Object RegExp object, RegExp slash', {'x':new RegExp('/')}, compare_Object(enumerate_props(compare_RegExp('\\/'))));
check('Object RegExp object, RegExp new line', {'x':new RegExp('\n')}, compare_Object(enumerate_props(compare_RegExp('\\n'))));

function compare_Error(actual, input) {
  assert_true(actual instanceof Error, "Checking instanceof");
  assert_equals(actual.constructor, input.constructor, "Checking constructor");
  assert_equals(actual.name, input.name, "Checking name");
  assert_equals(actual.hasOwnProperty("message"), input.hasOwnProperty("message"), "Checking message existence");
  assert_equals(actual.message, input.message, "Checking message");
  assert_equals(actual.foo, undefined, "Checking for absence of custom property");
}

check('Empty Error object', new Error, compare_Error);

const errorConstructors = [Error, EvalError, RangeError, ReferenceError,
                           SyntaxError, TypeError, URIError];
for (const constructor of errorConstructors) {
  check(`${constructor.name} object`, () => {
    let error = new constructor("Error message here");
    error.foo = "testing";
    return error;
  }, compare_Error);
}

async function compare_Blob(actual, input, expect_File) {
  if (typeof actual === 'string')
    assert_unreached(actual);
  assert_true(actual instanceof Blob, 'instanceof Blob');
  if (!expect_File)
    assert_false(actual instanceof File, 'instanceof File');
  assert_equals(actual.size, input.size, 'size');
  assert_equals(actual.type, input.type, 'type');
  assert_not_equals(actual, input);
  const ab1 = await new Response(actual).arrayBuffer();
  const ab2 = await new Response(input).arrayBuffer();
  assert_equals(ab1.byteLength, ab2.byteLength, 'byteLength');
  const ta1 = new Uint8Array(ab1);
  const ta2 = new Uint8Array(ab2);
  for(let i = 0; i < ta1.size; i++) {
    assert_equals(ta1[i], ta2[i]);
  }
}
function func_Blob_basic() {
  return new Blob(['foo'], {type:'text/x-bar'});
}
check('Blob basic', func_Blob_basic, compare_Blob);

function b(str) {
  return parseInt(str, 2);
}
function encode_cesu8(codeunits) {
  // http://www.unicode.org/reports/tr26/ section 2.2
  // only the 3-byte form is supported
  const rv = [];
  codeunits.forEach(function(codeunit) {
    rv.push(b('11100000') + ((codeunit & b('1111000000000000')) >> 12));
    rv.push(b('10000000') + ((codeunit & b('0000111111000000')) >> 6));
    rv.push(b('10000000') +  (codeunit & b('0000000000111111')));
  });
  return rv;
}
function func_Blob_bytes(arr) {
  return function() {
    const buffer = new ArrayBuffer(arr.length);
    const view = new DataView(buffer);
    for (let i = 0; i < arr.length; ++i) {
      view.setUint8(i, arr[i]);
    }
    return new Blob([view]);
  };
}
check('Blob unpaired high surrogate (invalid utf-8)', func_Blob_bytes(encode_cesu8([0xD800])), compare_Blob);
check('Blob unpaired low surrogate (invalid utf-8)', func_Blob_bytes(encode_cesu8([0xDC00])), compare_Blob);
check('Blob paired surrogates (invalid utf-8)', func_Blob_bytes(encode_cesu8([0xD800, 0xDC00])), compare_Blob);

function func_Blob_empty() {
  return new Blob(['']);
}
check('Blob empty', func_Blob_empty , compare_Blob);
function func_Blob_NUL() {
  return new Blob(['\u0000']);
}
check('Blob NUL', func_Blob_NUL, compare_Blob);

check('Array Blob object, Blob basic', [func_Blob_basic()], compare_Array(enumerate_props(compare_Blob)));
check('Array Blob object, Blob unpaired high surrogate (invalid utf-8)', [func_Blob_bytes([0xD800])()], compare_Array(enumerate_props(compare_Blob)));
check('Array Blob object, Blob unpaired low surrogate (invalid utf-8)', [func_Blob_bytes([0xDC00])()], compare_Array(enumerate_props(compare_Blob)));
check('Array Blob object, Blob paired surrogates (invalid utf-8)', [func_Blob_bytes([0xD800, 0xDC00])()], compare_Array(enumerate_props(compare_Blob)));
check('Array Blob object, Blob empty', [func_Blob_empty()], compare_Array(enumerate_props(compare_Blob)));
check('Array Blob object, Blob NUL', [func_Blob_NUL()], compare_Array(enumerate_props(compare_Blob)));
check('Array Blob object, two Blobs', [func_Blob_basic(), func_Blob_empty()], compare_Array(enumerate_props(compare_Blob)));

check('Object Blob object, Blob basic', {'x':func_Blob_basic()}, compare_Object(enumerate_props(compare_Blob)));
check('Object Blob object, Blob unpaired high surrogate (invalid utf-8)', {'x':func_Blob_bytes([0xD800])()}, compare_Object(enumerate_props(compare_Blob)));
check('Object Blob object, Blob unpaired low surrogate (invalid utf-8)', {'x':func_Blob_bytes([0xDC00])()}, compare_Object(enumerate_props(compare_Blob)));
check('Object Blob object, Blob paired surrogates (invalid utf-8)', {'x':func_Blob_bytes([0xD800, 0xDC00])()  }, compare_Object(enumerate_props(compare_Blob)));
check('Object Blob object, Blob empty', {'x':func_Blob_empty()}, compare_Object(enumerate_props(compare_Blob)));
check('Object Blob object, Blob NUL', {'x':func_Blob_NUL()}, compare_Object(enumerate_props(compare_Blob)));

async function compare_File(actual, input) {
  assert_true(actual instanceof File, 'instanceof File');
  assert_equals(actual.name, input.name, 'name');
  assert_equals(actual.lastModified, input.lastModified, 'lastModified');
  await compare_Blob(actual, input, true);
}
function func_File_basic() {
  return new File(['foo'], 'bar', {type:'text/x-bar', lastModified:42});
}
check('File basic', func_File_basic, compare_File);

function compare_FileList(actual, input) {
  if (typeof actual === 'string')
    assert_unreached(actual);
  assert_true(actual instanceof FileList, 'instanceof FileList');
  assert_equals(actual.length, input.length, 'length');
  assert_not_equals(actual, input);
  // XXX when there's a way to populate or construct a FileList,
  // check the items in the FileList
}
function func_FileList_empty() {
  const input = document.createElement('input');
  input.type = 'file';
  return input.files;
}
check('FileList empty', func_FileList_empty, compare_FileList, true);
check('Array FileList object, FileList empty', () => ([func_FileList_empty()]), compare_Array(enumerate_props(compare_FileList)), true);
check('Object FileList object, FileList empty', () => ({'x':func_FileList_empty()}), compare_Object(enumerate_props(compare_FileList)), true);

function compare_ArrayBuffer(actual, input) {
  assert_true(actual instanceof ArrayBuffer, 'instanceof ArrayBuffer');
  assert_equals(actual.byteLength, input.byteLength, 'byteLength');
  assert_equals(actual.maxByteLength, input.maxByteLength, 'maxByteLength');
  assert_equals(actual.resizable, input.resizable, 'resizable');
  assert_equals(actual.growable, input.growable, 'growable');
}

function compare_ArrayBufferView(view) {
  const Type = self[view];
  return function(actual, input) {
    if (typeof actual === 'string')
      assert_unreached(actual);
    assert_true(actual instanceof Type, 'instanceof '+view);
    assert_equals(actual.length, input.length, 'length');
    assert_equals(actual.byteLength, input.byteLength, 'byteLength');
    assert_equals(actual.byteOffset, input.byteOffset, 'byteOffset');
    assert_not_equals(actual.buffer, input.buffer, 'buffer');
    for (let i = 0; i < actual.length; ++i) {
      assert_equals(actual[i], input[i], 'actual['+i+']');
    }
  };
}
function compare_ImageData(actual, input) {
  if (typeof actual === 'string')
    assert_unreached(actual);
  assert_equals(actual.width, input.width, 'width');
  assert_equals(actual.height, input.height, 'height');
  assert_not_equals(actual.data, input.data, 'data');
  compare_ArrayBufferView('Uint8ClampedArray')(actual.data, input.data, null);
}
function func_ImageData_1x1_transparent_black() {
  const canvas = document.createElement('canvas');
  const ctx = canvas.getContext('2d');
  return ctx.createImageData(1, 1);
}
check('ImageData 1x1 transparent black', func_ImageData_1x1_transparent_black, compare_ImageData, true);
function func_ImageData_1x1_non_transparent_non_black() {
  const canvas = document.createElement('canvas');
  const ctx = canvas.getContext('2d');
  const imagedata = ctx.createImageData(1, 1);
  imagedata.data[0] = 100;
  imagedata.data[1] = 101;
  imagedata.data[2] = 102;
  imagedata.data[3] = 103;
  return imagedata;
}
check('ImageData 1x1 non-transparent non-black', func_ImageData_1x1_non_transparent_non_black, compare_ImageData, true);
check('Array ImageData object, ImageData 1x1 transparent black', () => ([func_ImageData_1x1_transparent_black()]), compare_Array(enumerate_props(compare_ImageData)), true);
check('Array ImageData object, ImageData 1x1 non-transparent non-black', () => ([func_ImageData_1x1_non_transparent_non_black()]), compare_Array(enumerate_props(compare_ImageData)), true);
check('Object ImageData object, ImageData 1x1 transparent black', () => ({'x':func_ImageData_1x1_transparent_black()}), compare_Object(enumerate_props(compare_ImageData)), true);
check('Object ImageData object, ImageData 1x1 non-transparent non-black', () => ({'x':func_ImageData_1x1_non_transparent_non_black()}), compare_Object(enumerate_props(compare_ImageData)), true);


check('Array sparse', new Array(10), compare_Array(enumerate_props(compare_primitive)));
check('Array with non-index property', function() {
  const rv = [];
  rv.foo = 'bar';
  return rv;
}, compare_Array(enumerate_props(compare_primitive)));
check('Object with index property and length', {'0':'foo', 'length':1}, compare_Object(enumerate_props(compare_primitive)));
function check_circular_property(prop) {
  return function(actual) {
    assert_equals(actual[prop], actual);
  };
}
check('Array with circular reference', function() {
  const rv = [];
  rv[0] = rv;
  return rv;
}, compare_Array(check_circular_property('0')));
check('Object with circular reference', function() {
  const rv = {};
  rv['x'] = rv;
  return rv;
}, compare_Object(check_circular_property('x')));
function check_identical_property_values(prop1, prop2) {
  return function(actual) {
    assert_equals(actual[prop1], actual[prop2]);
  };
}
check('Array with identical property values', function() {
  const obj = {}
  return [obj, obj];
}, compare_Array(check_identical_property_values('0', '1')));
check('Object with identical property values', function() {
  const obj = {}
  return {'x':obj, 'y':obj};
}, compare_Object(check_identical_property_values('x', 'y')));

function check_absent_property(prop) {
  return function(actual) {
    assert_false(prop in actual);
  };
}
check('Object with property on prototype', function() {
  const Foo = function() {};
  Foo.prototype = {'foo':'bar'};
  return new Foo();
}, compare_Object(check_absent_property('foo')));

check('Object with non-enumerable property', function() {
  const rv = {};
  Object.defineProperty(rv, 'foo', {value:'bar', enumerable:false, writable:true, configurable:true});
  return rv;
}, compare_Object(check_absent_property('foo')));

function check_writable_property(prop) {
  return function(actual, input) {
    assert_equals(actual[prop], input[prop]);
    actual[prop] += ' baz';
    assert_equals(actual[prop], input[prop] + ' baz');
  };
}
check('Object with non-writable property', function() {
  const rv = {};
  Object.defineProperty(rv, 'foo', {value:'bar', enumerable:true, writable:false, configurable:true});
  return rv;
}, compare_Object(check_writable_property('foo')));

function check_configurable_property(prop) {
  return function(actual, input) {
    assert_equals(actual[prop], input[prop]);
    delete actual[prop];
    assert_false('prop' in actual);
  };
}
check('Object with non-configurable property', function() {
  const rv = {};
  Object.defineProperty(rv, 'foo', {value:'bar', enumerable:true, writable:true, configurable:false});
  return rv;
}, compare_Object(check_configurable_property('foo')));

structuredCloneBatteryOfTests.push({
  description: 'Object with a getter that throws',
  async f(runner, t) {
    const exception = new Error();
    const testObject = {
      get testProperty() {
        throw exception;
      }
    };
    await promise_rejects_exactly(
      t,
      exception,
      runner.structuredClone(testObject)
    );
  }
});

/* The tests below are inspired by @zcorpan’s work but got some
more substantial changed due to their previous async setup */

function get_canvas_1x1_transparent_black() {
  const canvas = document.createElement('canvas');
  canvas.width = 1;
  canvas.height = 1;
  return canvas;
}

function get_canvas_1x1_non_transparent_non_black() {
  const canvas = document.createElement('canvas');
  canvas.width = 1;
  canvas.height = 1;
  const ctx = canvas.getContext('2d');
  const imagedata = ctx.getImageData(0, 0, 1, 1);
  imagedata.data[0] = 100;
  imagedata.data[1] = 101;
  imagedata.data[2] = 102;
  imagedata.data[3] = 103;
  return canvas;
}

function compare_ImageBitmap(actual, input) {
  if (typeof actual === 'string')
    assert_unreached(actual);
  assert_true(actual instanceof ImageBitmap, 'instanceof ImageBitmap');
  assert_not_equals(actual, input);
  // XXX paint the ImageBitmap on a canvas and check the data
}

structuredCloneBatteryOfTests.push({
  description: 'ImageBitmap 1x1 transparent black',
  async f(runner) {
    const canvas = get_canvas_1x1_transparent_black();
    const bm = await createImageBitmap(canvas);
    const copy = await runner.structuredClone(bm);
    compare_ImageBitmap(bm, copy);
  },
  requiresDocument: true
});

structuredCloneBatteryOfTests.push({
  description: 'ImageBitmap 1x1 non-transparent non-black',
  async f(runner) {
    const canvas = get_canvas_1x1_non_transparent_non_black();
    const bm = await createImageBitmap(canvas);
    const copy = await runner.structuredClone(bm);
    compare_ImageBitmap(bm, copy);
  },
  requiresDocument: true
});

structuredCloneBatteryOfTests.push({
  description: 'Array ImageBitmap object, ImageBitmap 1x1 transparent black',
  async f(runner) {
    const canvas = get_canvas_1x1_transparent_black();
    const bm = [await createImageBitmap(canvas)];
    const copy = await runner.structuredClone(bm);
    compare_Array(enumerate_props(compare_ImageBitmap))(bm, copy);
  },
  requiresDocument: true
});

structuredCloneBatteryOfTests.push({
  description: 'Array ImageBitmap object, ImageBitmap 1x1 transparent non-black',
  async f(runner) {
    const canvas = get_canvas_1x1_non_transparent_non_black();
    const bm = [await createImageBitmap(canvas)];
    const copy = await runner.structuredClone(bm);
    compare_Array(enumerate_props(compare_ImageBitmap))(bm, copy);
  },
  requiresDocument: true
});

structuredCloneBatteryOfTests.push({
  description: 'Object ImageBitmap object, ImageBitmap 1x1 transparent black',
  async f(runner) {
    const canvas = get_canvas_1x1_transparent_black();
    const bm = {x: await createImageBitmap(canvas)};
    const copy = await runner.structuredClone(bm);
    compare_Object(enumerate_props(compare_ImageBitmap))(bm, copy);
  },
  requiresDocument: true
});

structuredCloneBatteryOfTests.push({
  description: 'Object ImageBitmap object, ImageBitmap 1x1 transparent non-black',
  async f(runner) {
    const canvas = get_canvas_1x1_non_transparent_non_black();
    const bm = {x: await createImageBitmap(canvas)};
    const copy = await runner.structuredClone(bm);
    compare_Object(enumerate_props(compare_ImageBitmap))(bm, copy);
  },
  requiresDocument: true
});

check('ObjectPrototype must lose its exotic-ness when cloned',
  () => Object.prototype,
  (copy, original) => {
    assert_not_equals(copy, original);
    assert_true(copy instanceof Object);

    const newProto = { some: 'proto' };
    // Must not throw:
    Object.setPrototypeOf(copy, newProto);

    assert_equals(Object.getPrototypeOf(copy), newProto);
  }
);

structuredCloneBatteryOfTests.push({
  description: 'Serializing a non-serializable platform object fails',
  async f(runner, t) {
    const request = new Response();
    await promise_rejects_dom(
      t,
      "DataCloneError",
      runner.structuredClone(request)
    );
  }
});

structuredCloneBatteryOfTests.push({
  description: 'An object whose interface is deleted from the global must still deserialize',
  async f(runner) {
    const blob = new Blob();
    const blobInterface = globalThis.Blob;
    delete globalThis.Blob;
    try {
      const copy = await runner.structuredClone(blob);
      assert_true(copy instanceof blobInterface);
    } finally {
      globalThis.Blob = blobInterface;
    }
  }
});

check(
  'A subclass instance will deserialize as its closest serializable superclass',
  () => {
    class FileSubclass extends File {}
    return new FileSubclass([], "");
  },
  (copy) => {
    assert_equals(Object.getPrototypeOf(copy), File.prototype);
  }
);

check(
  'Resizable ArrayBuffer',
  () => {
    const ab = new ArrayBuffer(16, { maxByteLength: 1024 });
    assert_true(ab.resizable);
    return ab;
  },
  compare_ArrayBuffer);

structuredCloneBatteryOfTests.push({
  description: 'Growable SharedArrayBuffer',
  async f(runner) {
    const sab = createBuffer('SharedArrayBuffer', 16, { maxByteLength: 1024 });
    assert_true(sab.growable);
    try {
      const copy = await runner.structuredClone(sab);
      compare_ArrayBuffer(sab, copy);
    } catch (e) {
      // If we're cross-origin isolated, cloning SABs should not fail.
      if (e instanceof DOMException && e.code === DOMException.DATA_CLONE_ERR) {
        assert_false(self.crossOriginIsolated);
      } else {
        throw e;
      }
    }
  }
});

check(
  'Length-tracking TypedArray',
  () => {
    const ab = new ArrayBuffer(16, { maxByteLength: 1024 });
    assert_true(ab.resizable);
    return new Uint8Array(ab);
  },
  compare_ArrayBufferView('Uint8Array'));

check(
  'Length-tracking DataView',
  () => {
    const ab = new ArrayBuffer(16, { maxByteLength: 1024 });
    assert_true(ab.resizable);
    return new DataView(ab);
  },
  compare_ArrayBufferView('DataView'));

structuredCloneBatteryOfTests.push({
  description: 'Serializing OOB TypedArray throws',
  async f(runner, t) {
    const ab = new ArrayBuffer(16, { maxByteLength: 1024 });
    const ta = new Uint8Array(ab, 8);
    ab.resize(0);
    await promise_rejects_dom(
      t,
      "DataCloneError",
      runner.structuredClone(ta)
    );
  }
});

structuredCloneBatteryOfTests.push({
  description: 'Serializing OOB DataView throws',
  async f(runner, t) {
    const ab = new ArrayBuffer(16, { maxByteLength: 1024 });
    const dv = new DataView(ab, 8);
    ab.resize(0);
    await promise_rejects_dom(
      t,
      "DataCloneError",
      runner.structuredClone(dv)
    );
  }
});
