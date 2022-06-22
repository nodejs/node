const MAX_FRAME = 700;
const MAX_UNCHANGED_FRAMES = 20;

// Returns a promise that resolves when the given condition is met or rejects
// after MAX_FRAME animation frames.
function waitFor(condition, error_message = 'Reaches the maximum frames.') {
  return new Promise((resolve, reject) => {
    function tick(frames) {
      // We requestAnimationFrame either for MAX_FRAM frames or until condition
      // is met.
      if (frames >= MAX_FRAME)
        reject(error_message);
      else if (condition())
        resolve();
      else
        requestAnimationFrame(tick.bind(this, frames + 1));
    }
    tick(0);
  });
}

function waitForCompositorCommit() {
  return new Promise((resolve) => {
    // rAF twice.
    window.requestAnimationFrame(() => {
      window.requestAnimationFrame(resolve);
    });
  });
}

function waitForAnimationEnd(getValue) {
  var last_changed_frame = 0;
  var last_position = getValue();
  return new Promise((resolve, reject) => {
    function tick(frames) {
    // We requestAnimationFrame either for MAX_FRAME or until
    // MAX_UNCHANGED_FRAMES with no change have been observed.
      if (frames >= MAX_FRAME || frames - last_changed_frame > MAX_UNCHANGED_FRAMES) {
        resolve();
      } else {
        current_value = getValue();
        if (last_position != current_value) {
          last_changed_frame = frames;
          last_position = current_value;
        }
        requestAnimationFrame(tick.bind(this, frames + 1));
      }
    }
    tick(0);
  })
}

function touchScrollInTarget(pixels_to_scroll, target, direction, pause_time_in_ms = 100) {
  var x_delta = 0;
  var y_delta = 0;
  const num_movs = 5;
  if (direction == "down") {
    y_delta = -1 * pixels_to_scroll / num_movs;
  } else if (direction == "up") {
    y_delta = pixels_to_scroll / num_movs;
  } else if (direction == "right") {
    x_delta = -1 * pixels_to_scroll / num_movs;
  } else if (direction == "left") {
    x_delta = pixels_to_scroll / num_movs;;
  } else {
    throw("scroll direction '" + direction + "' is not expected, direction should be 'down', 'up', 'left' or 'right'");
  }
  return new test_driver.Actions()
      .addPointer("pointer1", "touch")
      .pointerMove(0, 0, {origin: target})
      .pointerDown()
      .pointerMove(x_delta, y_delta, {origin: target})
      .pointerMove(2 * x_delta, 2 * y_delta, {origin: target})
      .pointerMove(3 * x_delta, 3 * y_delta, {origin: target})
      .pointerMove(4 * x_delta, 4 * y_delta, {origin: target})
      .pointerMove(5 * x_delta, 5 * y_delta, {origin: target})
      .pause(pause_time_in_ms)
      .pointerUp()
      .send();
}

// Trigger fling by doing pointerUp right after pointerMoves.
function touchFlingInTarget(pixels_to_scroll, target, direction) {
  touchScrollInTarget(pixels_to_scroll, target, direction, 0 /* pause_time */);
}

function mouseActionsInTarget(target, origin, delta, pause_time_in_ms = 100) {
  return new test_driver.Actions()
    .addPointer("pointer1", "mouse")
    .pointerMove(origin.x, origin.y, { origin: target })
    .pointerDown()
    .pointerMove(origin.x + delta.x, origin.y + delta.y, { origin: target })
    .pointerMove(origin.x + delta.x * 2, origin.y + delta.y * 2, { origin: target })
    .pause(pause_time_in_ms)
    .pointerUp()
    .send();
}
