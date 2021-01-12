// META: global=window,worker
// META: title=Consuming Response body after getting a ReadableStream

promise_test(function() {
    return fetch("../resources/data.json").then(function(response) {
        response.blob();
        assert_not_equals(response.body, null);
        assert_throws_js(TypeError, function() { response.body.getReader(); });
    });
}, "Getting a body reader after consuming as blob");

promise_test(function() {
    return fetch("../resources/data.json").then(function(response) {
        response.text();
        assert_not_equals(response.body, null);
        assert_throws_js(TypeError, function() { response.body.getReader(); });
    });
}, "Getting a body reader after consuming as text");

promise_test(function() {
    return fetch("../resources/data.json").then(function(response) {
        response.json();
        assert_not_equals(response.body, null);
        assert_throws_js(TypeError, function() { response.body.getReader(); });
    });
}, "Getting a body reader after consuming as json");

promise_test(function() {
    return fetch("../resources/data.json").then(function(response) {
        response.arrayBuffer();
        assert_not_equals(response.body, null);
        assert_throws_js(TypeError, function() { response.body.getReader(); });
    });
}, "Getting a body reader after consuming as arrayBuffer");
