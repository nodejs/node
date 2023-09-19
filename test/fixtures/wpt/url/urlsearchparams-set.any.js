test(function() {
    var params = new URLSearchParams('a=b&c=d');
    params.set('a', 'B');
    assert_equals(params + '', 'a=B&c=d');
    params = new URLSearchParams('a=b&c=d&a=e');
    params.set('a', 'B');
    assert_equals(params + '', 'a=B&c=d')
    params.set('e', 'f');
    assert_equals(params + '', 'a=B&c=d&e=f')
}, 'Set basics');

test(function() {
    var params = new URLSearchParams('a=1&a=2&a=3');
    assert_true(params.has('a'), 'Search params object has name "a"');
    assert_equals(params.get('a'), '1', 'Search params object has name "a" with value "1"');
    params.set('first', 4);
    assert_true(params.has('a'), 'Search params object has name "a"');
    assert_equals(params.get('a'), '1', 'Search params object has name "a" with value "1"');
    params.set('a', 4);
    assert_true(params.has('a'), 'Search params object has name "a"');
    assert_equals(params.get('a'), '4', 'Search params object has name "a" with value "4"');
}, 'URLSearchParams.set');
