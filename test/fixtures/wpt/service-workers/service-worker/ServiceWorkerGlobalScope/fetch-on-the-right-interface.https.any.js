// META: title=fetch method on the right interface
// META: global=serviceworker

test(function() {
    assert_false(self.hasOwnProperty('fetch'), 'ServiceWorkerGlobalScope ' +
        'instance should not have "fetch" method as its property.');
    assert_inherits(self, 'fetch', 'ServiceWorkerGlobalScope should ' +
        'inherit "fetch" method.');
    assert_own_property(Object.getPrototypeOf(Object.getPrototypeOf(self)), 'fetch',
        'WorkerGlobalScope should have "fetch" propery in its prototype.');
    assert_equals(self.fetch, Object.getPrototypeOf(Object.getPrototypeOf(self)).fetch,
        'ServiceWorkerGlobalScope.fetch should be the same as ' +
        'WorkerGlobalScope.fetch.');
}, 'Fetch method on the right interface');
