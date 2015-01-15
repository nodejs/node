// Correct dot placement:
var x = window.some()
    .method()
    .calls();

// Wrong dots:
window
    .some()
    // With a comment in between.
    .method()
    .calls();

// Wrong plus operator:
var y = 'hello' +
    'world' +
    // With a comment in between.
    '!';

// Correct plus operator (untouched):
var y = 'hello' +
    'world';
