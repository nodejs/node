// The test will create 3 timeouts with their intervals increasing.
// If the timeouts execute in order then the test is PASS.
self.addEventListener('message', function(e) {
    setTimeout(function () { postMessage(1); }, 5);
    setTimeout(function () { postMessage(2); }, 10);
    setTimeout(function () { postMessage(3); }, 15);
}, false);
