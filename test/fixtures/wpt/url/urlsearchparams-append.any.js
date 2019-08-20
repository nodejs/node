test(function() {
    var params = new URLSearchParams();
    params.append('a', 'b');
    assert_equals(params + '', 'a=b');
    params.append('a', 'b');
    assert_equals(params + '', 'a=b&a=b');
    params.append('a', 'c');
    assert_equals(params + '', 'a=b&a=b&a=c');
}, 'Append same name');

test(function() {
    var params = new URLSearchParams();
    params.append('', '');
    assert_equals(params + '', '=');
    params.append('', '');
    assert_equals(params + '', '=&=');
}, 'Append empty strings');

test(function() {
    var params = new URLSearchParams();
    params.append(null, null);
    assert_equals(params + '', 'null=null');
    params.append(null, null);
    assert_equals(params + '', 'null=null&null=null');
}, 'Append null');

test(function() {
    var params = new URLSearchParams();
    params.append('first', 1);
    params.append('second', 2);
    params.append('third', '');
    params.append('first', 10);
    assert_true(params.has('first'), 'Search params object has name "first"');
    assert_equals(params.get('first'), '1', 'Search params object has name "first" with value "1"');
    assert_equals(params.get('second'), '2', 'Search params object has name "second" with value "2"');
    assert_equals(params.get('third'), '', 'Search params object has name "third" with value ""');
    params.append('first', 10);
    assert_equals(params.get('first'), '1', 'Search params object has name "first" with value "1"');
}, 'Append multiple');
