// META: title=File constructor

const to_string_obj = { toString: () => 'a string' };
const to_string_throws = { toString: () => { throw new Error('expected'); } };

test(function() {
  assert_true("File" in globalThis, "globalThis should have a File property.");
}, "File interface object exists");

test(t => {
  assert_throws_js(TypeError, () => new File(),
                   'Bits argument is required');
  assert_throws_js(TypeError, () => new File([]),
                   'Name argument is required');
}, 'Required arguments');

function test_first_argument(arg1, expectedSize, testName) {
  test(function() {
    var file = new File(arg1, "dummy");
    assert_true(file instanceof File);
    assert_equals(file.name, "dummy");
    assert_equals(file.size, expectedSize);
    assert_equals(file.type, "");
    // assert_false(file.isClosed); XXX: File.isClosed doesn't seem to be implemented
    assert_not_equals(file.lastModified, "");
  }, testName);
}

test_first_argument([], 0, "empty fileBits");
test_first_argument(["bits"], 4, "DOMString fileBits");
test_first_argument(["𝓽𝓮𝔁𝓽"], 16, "Unicode DOMString fileBits");
test_first_argument([new String('string object')], 13, "String object fileBits");
test_first_argument([new Blob()], 0, "Empty Blob fileBits");
test_first_argument([new Blob(["bits"])], 4, "Blob fileBits");
test_first_argument([new File([], 'world.txt')], 0, "Empty File fileBits");
test_first_argument([new File(["bits"], 'world.txt')], 4, "File fileBits");
test_first_argument([new ArrayBuffer(8)], 8, "ArrayBuffer fileBits");
test_first_argument([new Uint8Array([0x50, 0x41, 0x53, 0x53])], 4, "Typed array fileBits");
test_first_argument(["bits", new Blob(["bits"]), new Blob(), new Uint8Array([0x50, 0x41]),
                     new Uint16Array([0x5353]), new Uint32Array([0x53534150])], 16, "Various fileBits");
test_first_argument([12], 2, "Number in fileBits");
test_first_argument([[1,2,3]], 5, "Array in fileBits");
test_first_argument([{}], 15, "Object in fileBits"); // "[object Object]"
if (globalThis.document !== undefined) {
  test_first_argument([document.body], 24, "HTMLBodyElement in fileBits"); // "[object HTMLBodyElement]"
}
test_first_argument([to_string_obj], 8, "Object with toString in fileBits");
test_first_argument({[Symbol.iterator]() {
  let i = 0;
  return {next: () => [
    {done:false, value:'ab'},
    {done:false, value:'cde'},
    {done:true}
  ][i++]};
}}, 5, 'Custom @@iterator');

[
  'hello',
  0,
  null
].forEach(arg => {
  test(t => {
    assert_throws_js(TypeError, () => new File(arg, 'world.html'),
                     'Constructor should throw for invalid bits argument');
  }, `Invalid bits argument: ${JSON.stringify(arg)}`);
});

test(t => {
  assert_throws_js(Error, () => new File([to_string_throws], 'name.txt'),
                   'Constructor should propagate exceptions');
}, 'Bits argument: object that throws');


function test_second_argument(arg2, expectedFileName, testName) {
  test(function() {
    var file = new File(["bits"], arg2);
    assert_true(file instanceof File);
    assert_equals(file.name, expectedFileName);
  }, testName);
}

test_second_argument("dummy", "dummy", "Using fileName");
test_second_argument("dummy/foo", "dummy/foo",
                     "No replacement when using special character in fileName");
test_second_argument(null, "null", "Using null fileName");
test_second_argument(1, "1", "Using number fileName");
test_second_argument('', '', "Using empty string fileName");
if (globalThis.document !== undefined) {
  test_second_argument(document.body, '[object HTMLBodyElement]', "Using object fileName");
}

// testing the third argument
[
  {type: 'text/plain', expected: 'text/plain'},
  {type: 'text/plain;charset=UTF-8', expected: 'text/plain;charset=utf-8'},
  {type: 'TEXT/PLAIN', expected: 'text/plain'},
  {type: '𝓽𝓮𝔁𝓽/𝔭𝔩𝔞𝔦𝔫', expected: ''},
  {type: 'ascii/nonprintable\u001F', expected: ''},
  {type: 'ascii/nonprintable\u007F', expected: ''},
  {type: 'nonascii\u00EE', expected: ''},
  {type: 'nonascii\u1234', expected: ''},
  {type: 'nonparsable', expected: 'nonparsable'}
].forEach(testCase => {
  test(t => {
    var file = new File(["bits"], "dummy", { type: testCase.type});
    assert_true(file instanceof File);
    assert_equals(file.type, testCase.expected);
  }, `Using type in File constructor: ${testCase.type}`);
});
test(function() {
  var file = new File(["bits"], "dummy", { lastModified: 42 });
  assert_true(file instanceof File);
  assert_equals(file.lastModified, 42);
}, "Using lastModified");
test(function() {
  var file = new File(["bits"], "dummy", { name: "foo" });
  assert_true(file instanceof File);
  assert_equals(file.name, "dummy");
}, "Misusing name");
test(function() {
  var file = new File(["bits"], "dummy", { unknownKey: "value" });
  assert_true(file instanceof File);
  assert_equals(file.name, "dummy");
}, "Unknown properties are ignored");

[
  123,
  123.4,
  true,
  'abc'
].forEach(arg => {
  test(t => {
    assert_throws_js(TypeError, () => new File(['bits'], 'name.txt', arg),
                     'Constructor should throw for invalid property bag type');
  }, `Invalid property bag: ${JSON.stringify(arg)}`);
});

[
  null,
  undefined,
  [1,2,3],
  /regex/,
  function() {}
].forEach(arg => {
  test(t => {
    assert_equals(new File(['bits'], 'name.txt', arg).size, 4,
                  'Constructor should accept object-ish property bag type');
  }, `Unusual but valid property bag: ${arg}`);
});

test(t => {
  assert_throws_js(Error,
                   () => new File(['bits'], 'name.txt', {type: to_string_throws}),
                   'Constructor should propagate exceptions');
}, 'Property bag propagates exceptions');
