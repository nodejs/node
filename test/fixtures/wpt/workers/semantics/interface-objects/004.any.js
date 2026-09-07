// META: global=sharedworker

var unexpected = [
  // https://html.spec.whatwg.org/
  "ApplicationCache",
  "SharedWorker",
  "CanvasPath",
  "DedicatedWorkerGlobalScope",
  "AbstractView",
  "AbstractWorker",
  "Location",
  "Navigator",
  "DOMImplementation",
  "Audio",
  "HTMLCanvasElement",
  "Path",
  "CanvasProxy",
  "CanvasRenderingContext2D",
  "DrawingStyle",
  "PopStateEvent",
  "HashChangeEvent",
  "PageTransitionEvent",
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
];

for (var i = 0; i < unexpected.length; ++i) {
  test(function() {
    assert_false(unexpected[i] in self);
  }, "The " + unexpected[i] + " interface object should not be exposed");
}
