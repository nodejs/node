test(function() {
    var params = new URLSearchParams('a=b&c=d');
    assert_equals(params.get('a'), 'b');
    assert_equals(params.get('c'), 'd');
    assert_equals(params.get('e'), null);
    params = new URLSearchParams('a=b&c=d&a=e');
    assert_equals(params.get('a'), 'b');
    params = new URLSearchParams('=b&c=d');
    assert_equals(params.get(''), 'b');
    params = new URLSearchParams('a=&c=d&a=e');
    assert_equals(params.get('a'), '');
}, 'Get basics');

test(function() {
    var params = new URLSearchParams('first=second&third&&');
    assert_true(params != null, 'constructor returned non-null value.');
    assert_true(params.has('first'), 'Search params object has name "first"');
    assert_equals(params.get('first'), 'second', 'Search params object has name "first" with value "second"');
    assert_equals(params.get('third'), '', 'Search params object has name "third" with the empty value.');
    assert_equals(params.get('fourth'), null, 'Search params object has no "fourth" name and value.');
}, 'More get() basics');
