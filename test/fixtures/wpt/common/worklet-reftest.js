/**
 * Imports code into a worklet. E.g.
 *
 * importWorklet(CSS.paintWorklet, {url: 'script.js'});
 * importWorklet(CSS.paintWorklet, '(javascript string)');
 *
 * @param {Worklet} worklet
 * @param {(Object|string)} code
 */
function importWorklet(worklet, code) {
    let url;
    if (typeof code === 'object') {
      url = code.url;
    } else {
      const blob = new Blob([code], {type: 'text/javascript'});
      url = URL.createObjectURL(blob);
    }

    return worklet.addModule(url);
}

/** @private */
async function animationFrames(frames) {
  for (let i = 0; i < frames; i++)
    await new Promise(requestAnimationFrame);
}

/** @private */
async function workletPainted() {
    await animationFrames(2);
}

/**
 * To make sure that we take the snapshot at the right time, we do double
 * requestAnimationFrame. In the second frame, we take a screenshot, that makes
 * sure that we already have a full frame.
 *
 * @param {Worklet} worklet
 * @param {(Object|string)} code
 */
async function importWorkletAndTerminateTestAfterAsyncPaint(worklet, code) {
    if (typeof worklet === 'undefined') {
        takeScreenshot();
        return;
    }

    await importWorklet(worklet, code);
    await workletPainted();
    takeScreenshot();
}
