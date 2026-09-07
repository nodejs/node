importScripts('/resources/testharness.js');
test(t => {
    assert_equals(location.pathname, '/workers/interfaces/WorkerGlobalScope/location/redirect.js');
    assert_equals(location.search, '?a');
    assert_equals(location.hash, '');
});
done();
