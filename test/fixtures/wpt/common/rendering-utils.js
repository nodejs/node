"use strict";

/**
 * Waits until we have at least one frame rendered, regardless of the engine.
 *
 * @returns {Promise}
 */
function waitForAtLeastOneFrame() {
  return new Promise(resolve => {
    // Different web engines work slightly different on this area but waiting
    // for two requestAnimationFrames() to happen, one after another, should be
    // sufficient to ensure at least one frame has been generated anywhere.
    window.requestAnimationFrame(() => {
      window.requestAnimationFrame(() => {
        resolve();
      });
    });
  });
}
