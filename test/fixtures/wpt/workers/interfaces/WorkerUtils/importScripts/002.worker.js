importScripts("/resources/testharness.js");

test(function() {
  var ran = false;
  assert_throws_dom("SyntaxError", function() {
    importScripts('data:text/javascript,ran=true','http://foo bar');
  });
  assert_false(ran, 'first argument to importScripts ran');
});

done();
