test(function() {
    var params = new URLSearchParams();
    assert_equals(params + '', '');
    params = new URLSearchParams('');
    assert_equals(params + '', '');
    params = new URLSearchParams('a=b');
    assert_equals(params + '', 'a=b');
    params = new URLSearchParams(params);
    assert_equals(params + '', 'a=b');
}, 'Basic URLSearchParams construction');

test(function() {
    var params = new URLSearchParams()
    assert_equals(params.toString(), "")
}, "URLSearchParams constructor, no arguments")

test(function () {
    var params = new URLSearchParams("?a=b")
    assert_equals(params.toString(), "a=b")
}, 'URLSearchParams constructor, remove leading "?"')

test(() => {
    var params = new URLSearchParams(DOMException);
    assert_equals(params.toString(), "INDEX_SIZE_ERR=1&DOMSTRING_SIZE_ERR=2&HIERARCHY_REQUEST_ERR=3&WRONG_DOCUMENT_ERR=4&INVALID_CHARACTER_ERR=5&NO_DATA_ALLOWED_ERR=6&NO_MODIFICATION_ALLOWED_ERR=7&NOT_FOUND_ERR=8&NOT_SUPPORTED_ERR=9&INUSE_ATTRIBUTE_ERR=10&INVALID_STATE_ERR=11&SYNTAX_ERR=12&INVALID_MODIFICATION_ERR=13&NAMESPACE_ERR=14&INVALID_ACCESS_ERR=15&VALIDATION_ERR=16&TYPE_MISMATCH_ERR=17&SECURITY_ERR=18&NETWORK_ERR=19&ABORT_ERR=20&URL_MISMATCH_ERR=21&QUOTA_EXCEEDED_ERR=22&TIMEOUT_ERR=23&INVALID_NODE_TYPE_ERR=24&DATA_CLONE_ERR=25")
    assert_throws_js(TypeError, () => new URLSearchParams(DOMException.prototype),
                  "Constructing a URLSearchParams from DOMException.prototype should throw due to branding checks");
}, "URLSearchParams constructor, DOMException as argument")

test(() => {
    var params = new URLSearchParams('');
    assert_true(params != null, 'constructor returned non-null value.');
    assert_equals(Object.getPrototypeOf(params), URLSearchParams.prototype, 'expected URLSearchParams.prototype as prototype.');
}, "URLSearchParams constructor, empty string as argument")

test(() => {
    var params = new URLSearchParams({});
    assert_equals(params + '', "");
}, 'URLSearchParams constructor, {} as argument');

test(function() {
    var params = new URLSearchParams('a=b');
    assert_true(params != null, 'constructor returned non-null value.');
    assert_true(params.has('a'), 'Search params object has name "a"');
    assert_false(params.has('b'), 'Search params object has not got name "b"');

    params = new URLSearchParams('a=b&c');
    assert_true(params != null, 'constructor returned non-null value.');
    assert_true(params.has('a'), 'Search params object has name "a"');
    assert_true(params.has('c'), 'Search params object has name "c"');

    params = new URLSearchParams('&a&&& &&&&&a+b=& c&m%c3%b8%c3%b8');
    assert_true(params != null, 'constructor returned non-null value.');
    assert_true(params.has('a'), 'Search params object has name "a"');
    assert_true(params.has('a b'), 'Search params object has name "a b"');
    assert_true(params.has(' '), 'Search params object has name " "');
    assert_false(params.has('c'), 'Search params object did not have the name "c"');
    assert_true(params.has(' c'), 'Search params object has name " c"');
    assert_true(params.has('møø'), 'Search params object has name "møø"');

    params = new URLSearchParams('id=0&value=%');
    assert_true(params != null, 'constructor returned non-null value.');
    assert_true(params.has('id'), 'Search params object has name "id"');
    assert_true(params.has('value'), 'Search params object has name "value"');
    assert_equals(params.get('id'), '0');
    assert_equals(params.get('value'), '%');

    params = new URLSearchParams('b=%2sf%2a');
    assert_true(params != null, 'constructor returned non-null value.');
    assert_true(params.has('b'), 'Search params object has name "b"');
    assert_equals(params.get('b'), '%2sf*');

    params = new URLSearchParams('b=%2%2af%2a');
    assert_true(params != null, 'constructor returned non-null value.');
    assert_true(params.has('b'), 'Search params object has name "b"');
    assert_equals(params.get('b'), '%2*f*');

    params = new URLSearchParams('b=%%2a');
    assert_true(params != null, 'constructor returned non-null value.');
    assert_true(params.has('b'), 'Search params object has name "b"');
    assert_equals(params.get('b'), '%*');
}, 'URLSearchParams constructor, string.');

test(function() {
    var seed = new URLSearchParams('a=b&c=d');
    var params = new URLSearchParams(seed);
    assert_true(params != null, 'constructor returned non-null value.');
    assert_equals(params.get('a'), 'b');
    assert_equals(params.get('c'), 'd');
    assert_false(params.has('d'));
    // The name-value pairs are copied when created; later updates
    // should not be observable.
    seed.append('e', 'f');
    assert_false(params.has('e'));
    params.append('g', 'h');
    assert_false(seed.has('g'));
}, 'URLSearchParams constructor, object.');

test(function() {
    var formData = new FormData()
    formData.append('a', 'b')
    formData.append('c', 'd')
    var params = new URLSearchParams(formData);
    assert_true(params != null, 'constructor returned non-null value.');
    assert_equals(params.get('a'), 'b');
    assert_equals(params.get('c'), 'd');
    assert_false(params.has('d'));
    // The name-value pairs are copied when created; later updates
    // should not be observable.
    formData.append('e', 'f');
    assert_false(params.has('e'));
    params.append('g', 'h');
    assert_false(formData.has('g'));
}, 'URLSearchParams constructor, FormData.');

test(function() {
    var params = new URLSearchParams('a=b+c');
    assert_equals(params.get('a'), 'b c');
    params = new URLSearchParams('a+b=c');
    assert_equals(params.get('a b'), 'c');
}, 'Parse +');

test(function() {
    const testValue = '+15555555555';
    const params = new URLSearchParams();
    params.set('query', testValue);
    var newParams = new URLSearchParams(params.toString());

    assert_equals(params.toString(), 'query=%2B15555555555');
    assert_equals(params.get('query'), testValue);
    assert_equals(newParams.get('query'), testValue);
}, 'Parse encoded +');

test(function() {
    var params = new URLSearchParams('a=b c');
    assert_equals(params.get('a'), 'b c');
    params = new URLSearchParams('a b=c');
    assert_equals(params.get('a b'), 'c');
}, 'Parse space');

test(function() {
    var params = new URLSearchParams('a=b%20c');
    assert_equals(params.get('a'), 'b c');
    params = new URLSearchParams('a%20b=c');
    assert_equals(params.get('a b'), 'c');
}, 'Parse %20');

test(function() {
    var params = new URLSearchParams('a=b\0c');
    assert_equals(params.get('a'), 'b\0c');
    params = new URLSearchParams('a\0b=c');
    assert_equals(params.get('a\0b'), 'c');
}, 'Parse \\0');

test(function() {
    var params = new URLSearchParams('a=b%00c');
    assert_equals(params.get('a'), 'b\0c');
    params = new URLSearchParams('a%00b=c');
    assert_equals(params.get('a\0b'), 'c');
}, 'Parse %00');

test(function() {
    var params = new URLSearchParams('a=b\u2384');
    assert_equals(params.get('a'), 'b\u2384');
    params = new URLSearchParams('a\u2384b=c');
    assert_equals(params.get('a\u2384b'), 'c');
}, 'Parse \u2384');  // Unicode Character 'COMPOSITION SYMBOL' (U+2384)

test(function() {
    var params = new URLSearchParams('a=b%e2%8e%84');
    assert_equals(params.get('a'), 'b\u2384');
    params = new URLSearchParams('a%e2%8e%84b=c');
    assert_equals(params.get('a\u2384b'), 'c');
}, 'Parse %e2%8e%84');  // Unicode Character 'COMPOSITION SYMBOL' (U+2384)

test(function() {
    var params = new URLSearchParams('a=b\uD83D\uDCA9c');
    assert_equals(params.get('a'), 'b\uD83D\uDCA9c');
    params = new URLSearchParams('a\uD83D\uDCA9b=c');
    assert_equals(params.get('a\uD83D\uDCA9b'), 'c');
}, 'Parse \uD83D\uDCA9');  // Unicode Character 'PILE OF POO' (U+1F4A9)

test(function() {
    var params = new URLSearchParams('a=b%f0%9f%92%a9c');
    assert_equals(params.get('a'), 'b\uD83D\uDCA9c');
    params = new URLSearchParams('a%f0%9f%92%a9b=c');
    assert_equals(params.get('a\uD83D\uDCA9b'), 'c');
}, 'Parse %f0%9f%92%a9');  // Unicode Character 'PILE OF POO' (U+1F4A9)

test(function() {
    var params = new URLSearchParams([]);
    assert_true(params != null, 'constructor returned non-null value.');
    params = new URLSearchParams([['a', 'b'], ['c', 'd']]);
    assert_equals(params.get("a"), "b");
    assert_equals(params.get("c"), "d");
    assert_throws_js(TypeError, function() { new URLSearchParams([[1]]); });
    assert_throws_js(TypeError, function() { new URLSearchParams([[1,2,3]]); });
}, "Constructor with sequence of sequences of strings");

[
  { "input": {"+": "%C2"}, "output": [["+", "%C2"]], "name": "object with +" },
  { "input": {c: "x", a: "?"}, "output": [["c", "x"], ["a", "?"]], "name": "object with two keys" },
  { "input": [["c", "x"], ["a", "?"]], "output": [["c", "x"], ["a", "?"]], "name": "array with two keys" },
  { "input": {"\uD835x": "1", "xx": "2", "\uD83Dx": "3"}, "output": [["\uFFFDx", "3"], ["xx", "2"]], "name": "2 unpaired surrogates (no trailing)" },
  { "input": {"x\uDC53": "1", "x\uDC5C": "2", "x\uDC65": "3"}, "output": [["x\uFFFD", "3"]], "name": "3 unpaired surrogates (no leading)" },
  { "input": {"a\0b": "42", "c\uD83D": "23", "d\u1234": "foo"}, "output": [["a\0b", "42"], ["c\uFFFD", "23"], ["d\u1234", "foo"]], "name": "object with NULL, non-ASCII, and surrogate keys" }
].forEach((val) => {
    test(() => {
        let params = new URLSearchParams(val.input),
            i = 0
        for (let param of params) {
            assert_array_equals(param, val.output[i])
            i++
        }
    }, "Construct with " + val.name)
})

test(() => {
  var params = new URLSearchParams()
  params[Symbol.iterator] = function *() {
    yield ["a", "b"]
  }
  let params2 = new URLSearchParams(params)
  assert_equals(params2.get("a"), "b")
}, "Custom [Symbol.iterator]")
