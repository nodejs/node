// Copyright (C) 2018 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

import {Animation} from './animation';
import {DragGestureHandler} from './drag_gesture_handler';
import {globals} from './globals';
import {handleKey} from './keyboard_event_handler';

// When first starting to pan or zoom, move at least this many units.
const INITIAL_PAN_STEP_PX = 50;
const INITIAL_ZOOM_STEP = 0.1;

// The snappiness (spring constant) of pan and zoom animations [0..1].
const SNAP_FACTOR = 0.4;

// How much the velocity of a pan or zoom animation increases per millisecond.
const ACCELERATION_PER_MS = 1 / 50;

// The default duration of a pan or zoom animation. The animation may run longer
// if the user keeps holding the respective button down or shorter if the button
// is released. This value so chosen so that it is longer than the typical key
// repeat timeout to avoid breaks in the animation.
const DEFAULT_ANIMATION_DURATION = 700;

// The minimum number of units to pan or zoom per frame (before the
// ACCELERATION_PER_MS multiplier is applied).
const ZOOM_RATIO_PER_FRAME = 0.008;
const KEYBOARD_PAN_PX_PER_FRAME = 8;

// Scroll wheel animation steps.
const HORIZONTAL_WHEEL_PAN_SPEED = 1;
const WHEEL_ZOOM_SPEED = -0.02;

const EDITING_RANGE_CURSOR = 'ew-resize';
const DRAG_CURSOR = 'default';
const PAN_CURSOR = 'move';

enum Pan {
  None = 0,
  Left = -1,
  Right = 1
}
function keyToPan(e: KeyboardEvent): Pan {
  const key = e.key.toLowerCase();
  if (['a'].includes(key)) return Pan.Left;
  if (['d', 'e'].includes(key)) return Pan.Right;
  return Pan.None;
}

enum Zoom {
  None = 0,
  In = 1,
  Out = -1
}
function keyToZoom(e: KeyboardEvent): Zoom {
  const key = e.key.toLowerCase();
  if (['w', ','].includes(key)) return Zoom.In;
  if (['s', 'o'].includes(key)) return Zoom.Out;
  return Zoom.None;
}

/**
 * Enables horizontal pan and zoom with mouse-based drag and WASD navigation.
 */
export class PanAndZoomHandler {
  private mousePositionX: number|null = null;
  private boundOnMouseMove = this.onMouseMove.bind(this);
  private boundOnWheel = this.onWheel.bind(this);
  private boundOnKeyDown = this.onKeyDown.bind(this);
  private boundOnKeyUp = this.onKeyUp.bind(this);
  private shiftDown = false;
  private panning: Pan = Pan.None;
  private panOffsetPx = 0;
  private targetPanOffsetPx = 0;
  private zooming: Zoom = Zoom.None;
  private zoomRatio = 0;
  private targetZoomRatio = 0;
  private panAnimation = new Animation(this.onPanAnimationStep.bind(this));
  private zoomAnimation = new Animation(this.onZoomAnimationStep.bind(this));

  private element: HTMLElement;
  private contentOffsetX: number;
  private onPanned: (movedPx: number) => void;
  private onZoomed: (zoomPositionPx: number, zoomRatio: number) => void;
  private editSelection: (currentPx: number) => boolean;
  private onSelection:
      (dragStartX: number, dragStartY: number, prevX: number, currentX: number,
       currentY: number, editing: boolean) => void;
  private selectingStarted: () => void;
  private selectingEnded: () => void;

  constructor({
    element,
    contentOffsetX,
    onPanned,
    onZoomed,
    editSelection,
    onSelection,
    selectingStarted,
    selectingEnded
  }: {
    element: HTMLElement,
    contentOffsetX: number,
    onPanned: (movedPx: number) => void,
    onZoomed: (zoomPositionPx: number, zoomRatio: number) => void,
    editSelection: (currentPx: number) => boolean,
    onSelection:
        (dragStartX: number, dragStartY: number, prevX: number,
         currentX: number, currentY: number, editing: boolean) => void,
    selectingStarted: () => void,
    selectingEnded: () => void,
  }) {
    this.element = element;
    this.contentOffsetX = contentOffsetX;
    this.onPanned = onPanned;
    this.onZoomed = onZoomed;
    this.editSelection = editSelection;
    this.onSelection = onSelection;
    this.selectingStarted = selectingStarted;
    this.selectingEnded = selectingEnded;

    document.body.addEventListener('keydown', this.boundOnKeyDown);
    document.body.addEventListener('keyup', this.boundOnKeyUp);
    this.element.addEventListener('mousemove', this.boundOnMouseMove);
    this.element.addEventListener('wheel', this.boundOnWheel, {passive: true});

    let prevX = -1;
    let dragStartX = -1;
    let dragStartY = -1;
    let edit = false;
    new DragGestureHandler(
        this.element,
        (x, y) => {
          if (this.shiftDown) {
            this.onPanned(prevX - x);
          } else {
            this.onSelection(dragStartX, dragStartY, prevX, x, y, edit);
          }
          prevX = x;
        },
        (x, y) => {
          prevX = x;
          dragStartX = x;
          dragStartY = y;
          edit = this.editSelection(x);
          // Set the cursor style based on where the cursor is when the drag
          // starts.
          if (edit) {
            this.element.style.cursor = EDITING_RANGE_CURSOR;
          } else if (!this.shiftDown) {
            this.selectingStarted();
            this.element.style.cursor = DRAG_CURSOR;
          }
        },
        () => {
          // Reset the cursor now the drag has ended.
          this.element.style.cursor = this.shiftDown ? PAN_CURSOR : DRAG_CURSOR;
          dragStartX = -1;
          dragStartY = -1;
          this.selectingEnded();
        });
  }


  shutdown() {
    document.body.removeEventListener('keydown', this.boundOnKeyDown);
    document.body.removeEventListener('keyup', this.boundOnKeyUp);
    this.element.removeEventListener('mousemove', this.boundOnMouseMove);
    this.element.removeEventListener('wheel', this.boundOnWheel);
  }

  private onPanAnimationStep(msSinceStartOfAnimation: number) {
    const step = (this.targetPanOffsetPx - this.panOffsetPx) * SNAP_FACTOR;
    if (this.panning !== Pan.None) {
      const velocity = 1 + msSinceStartOfAnimation * ACCELERATION_PER_MS;
      // Pan at least as fast as the snapping animation to avoid a
      // discontinuity.
      const targetStep = Math.max(KEYBOARD_PAN_PX_PER_FRAME * velocity, step);
      this.targetPanOffsetPx += this.panning * targetStep;
    }
    this.panOffsetPx += step;
    if (Math.abs(step) > 1e-1) {
      this.onPanned(step);
    } else {
      this.panAnimation.stop();
    }
  }

  private onZoomAnimationStep(msSinceStartOfAnimation: number) {
    if (this.mousePositionX === null) return;
    const step = (this.targetZoomRatio - this.zoomRatio) * SNAP_FACTOR;
    if (this.zooming !== Zoom.None) {
      const velocity = 1 + msSinceStartOfAnimation * ACCELERATION_PER_MS;
      // Zoom at least as fast as the snapping animation to avoid a
      // discontinuity.
      const targetStep = Math.max(ZOOM_RATIO_PER_FRAME * velocity, step);
      this.targetZoomRatio += this.zooming * targetStep;
    }
    this.zoomRatio += step;
    if (Math.abs(step) > 1e-6) {
      this.onZoomed(this.mousePositionX, step);
    } else {
      this.zoomAnimation.stop();
    }
  }

  private onMouseMove(e: MouseEvent) {
    const pageOffset =
        globals.frontendLocalState.sidebarVisible ? this.contentOffsetX : 0;
    // We can't use layerX here because there are many layers in this element.
    this.mousePositionX = e.clientX - pageOffset;
    // Only change the cursor when hovering, the DragGestureHandler handles
    // changing the cursor during drag events. This avoids the problem of
    // the cursor flickering between styles if you drag fast and get too
    // far from the current time range.
    if (e.buttons === 0) {
      if (this.editSelection(this.mousePositionX)) {
        this.element.style.cursor = EDITING_RANGE_CURSOR;
      } else {
        this.element.style.cursor = this.shiftDown ? PAN_CURSOR : DRAG_CURSOR;
      }
    }
  }

  private onWheel(e: WheelEvent) {
    if (Math.abs(e.deltaX) > Math.abs(e.deltaY)) {
      this.onPanned(e.deltaX * HORIZONTAL_WHEEL_PAN_SPEED);
      globals.rafScheduler.scheduleRedraw();
    } else if (e.ctrlKey && this.mousePositionX) {
      const sign = e.deltaY < 0 ? -1 : 1;
      const deltaY = sign * Math.log2(1 + Math.abs(e.deltaY));
      this.onZoomed(this.mousePositionX, deltaY * WHEEL_ZOOM_SPEED);
      globals.rafScheduler.scheduleRedraw();
    }
  }

  private onKeyDown(e: KeyboardEvent) {
    this.updateShift(e.shiftKey);
    if (keyToPan(e) !== Pan.None) {
      if (this.panning !== keyToPan(e)) {
        this.panAnimation.stop();
        this.panOffsetPx = 0;
        this.targetPanOffsetPx = keyToPan(e) * INITIAL_PAN_STEP_PX;
      }
      this.panning = keyToPan(e);
      this.panAnimation.start(DEFAULT_ANIMATION_DURATION);
    }

    if (keyToZoom(e) !== Zoom.None) {
      if (this.zooming !== keyToZoom(e)) {
        this.zoomAnimation.stop();
        this.zoomRatio = 0;
        this.targetZoomRatio = keyToZoom(e) * INITIAL_ZOOM_STEP;
      }
      this.zooming = keyToZoom(e);
      this.zoomAnimation.start(DEFAULT_ANIMATION_DURATION);
    }

    // Handle key events that are not pan or zoom.
    handleKey(e, true);
  }

  private onKeyUp(e: KeyboardEvent) {
    this.updateShift(e.shiftKey);
    if (keyToPan(e) === this.panning) {
      this.panning = Pan.None;
    }
    if (keyToZoom(e) === this.zooming) {
      this.zooming = Zoom.None;
    }

    // Handle key events that are not pan or zoom.
    handleKey(e, false);
  }

  // TODO(taylori): Move this shift handling into the viewer page.
  private updateShift(down: boolean) {
    if (down === this.shiftDown) return;
    this.shiftDown = down;
    if (this.shiftDown) {
      this.element.style.cursor = PAN_CURSOR;
    } else if (this.mousePositionX) {
      this.element.style.cursor = DRAG_CURSOR;
    }
  }
}
