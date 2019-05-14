test(function() {
    var params = new URLSearchParams('a=1&b=2&c=3');
    var keys = [];
    var values = [];
    params.forEach(function(value, key) {
        keys.push(key);
        values.push(value);
    });
    assert_array_equals(keys, ['a', 'b', 'c']);
    assert_array_equals(values, ['1', '2', '3']);
}, "ForEach Check");

test(function() {
    let a = new URL("http://a.b/c?a=1&b=2&c=3&d=4");
    let b = a.searchParams;
    var c = [];
    for (i of b) {
        a.search = "x=1&y=2&z=3";
        c.push(i);
    }
    assert_array_equals(c[0], ["a","1"]);
    assert_array_equals(c[1], ["y","2"]);
    assert_array_equals(c[2], ["z","3"]);
}, "For-of Check");

test(function() {
    let a = new URL("http://a.b/c");
    let b = a.searchParams;
    for (i of b) {
        assert_unreached(i);
    }
}, "empty");

test(function() {
    const url = new URL("http://localhost/query?param0=0&param1=1&param2=2");
    const searchParams = url.searchParams;
    const seen = [];
    for (param of searchParams) {
        if (param[0] === 'param0') {
            searchParams.delete('param1');
        }
        seen.push(param);
    }

    assert_array_equals(seen[0], ["param0", "0"]);
    assert_array_equals(seen[1], ["param2", "2"]);
}, "delete next param during iteration");

test(function() {
    const url = new URL("http://localhost/query?param0=0&param1=1&param2=2");
    const searchParams = url.searchParams;
    const seen = [];
    for (param of searchParams) {
        if (param[0] === 'param0') {
            searchParams.delete('param0');
            // 'param1=1' is now in the first slot, so the next iteration will see 'param2=2'.
        } else {
            seen.push(param);
        }
    }

    assert_array_equals(seen[0], ["param2", "2"]);
}, "delete current param during iteration");

test(function() {
    const url = new URL("http://localhost/query?param0=0&param1=1&param2=2");
    const searchParams = url.searchParams;
    const seen = [];
    for (param of searchParams) {
        seen.push(param[0]);
        searchParams.delete(param[0]);
    }

    assert_array_equals(seen, ["param0", "param2"], "param1 should not have been seen by the loop");
    assert_equals(String(searchParams), "param1=1", "param1 should remain");
}, "delete every param seen during iteration");
