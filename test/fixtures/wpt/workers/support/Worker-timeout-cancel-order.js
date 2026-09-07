// The test will create 2 timeouts and cancel the first one. If only the second
// timeout executes then the test passes.
self.addEventListener('message', function(e) {
    var t1 = setTimeout(function () { postMessage(1); }, 5);
    setTimeout(function () { postMessage(2); }, 10);
    clearTimeout(t1);
}, false);
