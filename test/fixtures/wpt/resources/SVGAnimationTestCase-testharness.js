// NOTE(edvardt):
// This file is a slimmed down wrapper for the old SVGAnimationTestCase.js,
// it has some convenience functions and should not be used for new tests.
// New tests should not build on this API as it's just meant to keep things
// working.

// Helper functions
const xlinkNS = "http://www.w3.org/1999/xlink"

function expectFillColor(element, red, green, blue, message) {
    let color = window.getComputedStyle(element, null).fill;
    var re = new RegExp("rgba?\\(([^, ]*), ([^, ]*), ([^, ]*)(?:, )?([^, ]*)\\)");
    rgb = re.exec(color);

    assert_approx_equals(Number(rgb[1]), red, 2.0, message);
    assert_approx_equals(Number(rgb[2]), green, 2.0, message);
    assert_approx_equals(Number(rgb[3]), blue, 2.0, message);
}

function expectColor(element, red, green, blue, property) {
    if (typeof property != "string")
      color = getComputedStyle(element).getPropertyValue("color");
    else
      color = getComputedStyle(element).getPropertyValue(property);
    var re = new RegExp("rgba?\\(([^, ]*), ([^, ]*), ([^, ]*)(?:, )?([^, ]*)\\)");
    rgb = re.exec(color);
    assert_approx_equals(Number(rgb[1]), red, 2.0);
    assert_approx_equals(Number(rgb[2]), green, 2.0);
    assert_approx_equals(Number(rgb[3]), blue, 2.0);
}

function createSVGElement(type) {
  return document.createElementNS("http://www.w3.org/2000/svg", type);
}

// Inspired by Layoutests/animations/animation-test-helpers.js
function moveAnimationTimelineAndSample(index) {
    var animationId = expectedResults[index][0];
    var time = expectedResults[index][1];
    var sampleCallback = expectedResults[index][2];
    var animation = rootSVGElement.ownerDocument.getElementById(animationId);

    // If we want to sample the animation end, add a small delta, to reliable point past the end of the animation.
    newTime = time;

    // The sample time is relative to the start time of the animation, take that into account.
    rootSVGElement.setCurrentTime(newTime);

    // NOTE(edvardt):
    // This is a dumb hack, some of the old tests sampled before the animation start, this
    // isn't technically part of the animation tests and is "impossible" to translate since
    // tests start automatically. Thus I solved it by skipping it.
    if (time != 0.0)
        sampleCallback();
}

var currentTest = 0;
var expectedResults;

function sampleAnimation(t) {
    if (currentTest == expectedResults.length) {
        t.done();
        return;
    }

    moveAnimationTimelineAndSample(currentTest);
    ++currentTest;

    step_timeout(t.step_func(function () { sampleAnimation(t); }), 0);
}

function runAnimationTest(t, expected) {
    if (!expected)
        throw("Expected results are missing!");
    if (currentTest > 0)
        throw("Not allowed to call runAnimationTest() twice");

    expectedResults = expected;
    testCount = expectedResults.length;
    currentTest = 0;

    step_timeout(t.step_func(function () { sampleAnimation(this); }), 50);
}

function smil_async_test(func) {
  async_test(t => {
    window.onload = t.step_func(function () {
      // Pause animations, we'll drive them manually.
      // This also ensures that the timeline is paused before
      // it starts. This should make the instance time of the below
      // 'click' (for instance) 0, and hence minimize rounding
      // errors for the addition in moveAnimationTimelineAndSample.
      rootSVGElement.pauseAnimations();

      // If eg. an animation is running with begin="0s", and
      // we want to sample the first time, before the animation
      // starts, then we can't delay the testing by using an
      // onclick event, as the animation would be past start time.
      func(t);
    });
  });
}
