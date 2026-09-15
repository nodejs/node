importScripts("/resources/testharness.js");
var unexpected = [
  // https://html.spec.whatwg.org/
  "SharedWorker",
  "CanvasPath",
  "SharedWorkerGlobalScope",
  "AbstractView",
  "AbstractWorker",
  "ApplicationCache",
  "Location",
  "Navigator",
  "Audio",
  "HTMLCanvasElement",
  "Path",
  "CanvasProxy",
  "CanvasRenderingContext2D",
  "DrawingStyle",
  "BeforeUnloadEvent",
  "PopStateEvent",
  "HashChangeEvent",
  "PageTransitionEvent",
  // https://dom.spec.whatwg.org/
  "DOMImplementation",
  // http://w3c.github.io/IndexedDB/
  "IDBEnvironment",
  // https://www.w3.org/TR/2010/NOTE-webdatabase-20101118/
  "Database",
  // https://w3c.github.io/uievents/
  "UIEvent",
  "FocusEvent",
  "MouseEvent",
  "WheelEvent",
  "InputEvent",
  "KeyboardEvent",
  "CompositionEvent",
  // https://w3c.github.io/webvtt/
  "VTTCue",
  "VTTRegion",
];
for (var i = 0; i < unexpected.length; ++i) {
  test(function () {
    assert_false(unexpected[i] in self);
  }, "The " + unexpected[i] + " interface object should not be exposed.");
}
done();
