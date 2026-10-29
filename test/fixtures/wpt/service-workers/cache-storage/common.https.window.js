// META: title=Cache Storage: Verify that Window and Workers see same storage
// META: timeout=long

function wait_for_message(worker) {
    return new Promise(function(resolve) {
        worker.addEventListener('message', function listener(e) {
            resolve(e.data);
            worker.removeEventListener('message', listener);
        });
    });
}

promise_test(function(t) {
    var cache_name = 'common-test';
    return self.caches.delete(cache_name)
        .then(function() {
            var worker = new Worker('resources/common-worker.js');
            worker.postMessage({name: cache_name});
            return wait_for_message(worker);
        })
        .then(function(message) {
            return self.caches.open(cache_name);
        })
        .then(function(cache) {
            return Promise.all([
                cache.match('https://example.com/a'),
                cache.match('https://example.com/b'),
                cache.match('https://example.com/c')
            ]);
        })
        .then(function(responses) {
            return Promise.all(responses.map(
                function(response) { return response.text(); }
            ));
        })
        .then(function(bodies) {
            assert_equals(bodies[0], 'a',
                          'Body should match response put by worker');
            assert_equals(bodies[1], 'b',
                          'Body should match response put by worker');
            assert_equals(bodies[2], 'c',
                          'Body should match response put by worker');
        });
}, 'Window sees cache puts by Worker');
