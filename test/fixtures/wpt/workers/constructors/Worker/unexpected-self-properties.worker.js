importScripts("/resources/testharness.js");

var unexpected = ['open', 'print', 'stop', 'getComputedStyle', 'getSelection', 'releaseEvents', 'captureEvents', 'alert', 'confirm', 'prompt', 'addEventStream', 'removeEventStream', 'back', 'forward', 'attachEvent', 'detachEvent', 'navigate', 'DOMParser', 'XMLSerializer', 'XPathEvaluator', 'XSLTProcessor', 'opera', 'Image', 'Option', 'frames', 'Audio', 'SVGUnitTypes', 'SVGZoomAndPan', 'java', 'netscape', 'sun', 'Packages', 'ByteArray', 'closed', 'defaultStatus', 'document', 'event', 'frameElement', 'history', 'innerHeight', 'innerWidth', 'opener', 'outerHeight', 'outerWidth', 'pageXOffset', 'pageYOffset', 'parent', 'screen', 'screenLeft', 'screenTop', 'screenX', 'screenY', 'status', 'top', 'window', 'length', 'SharedWorker']; // iterated window in opera and removed expected ones
for (var i = 0; i < unexpected.length; ++i) {
  var property = unexpected[i];
  test(function() {
    assert_false(property in self);
  }, "existence of " + property);
}

done();
