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

export class DragGestureHandler {
  private readonly boundOnMouseDown = this.onMouseDown.bind(this);
  private readonly boundOnMouseMove = this.onMouseMove.bind(this);
  private readonly boundOnMouseUp = this.onMouseUp.bind(this);
  private clientRect?: DOMRect;
  private pendingMouseDownEvent?: MouseEvent;

  constructor(
      private element: HTMLElement,
      private onDrag: (x: number, y: number) => void,
      private onDragStarted: (x: number, y: number) => void = () => {},
      private onDragFinished = () => {}) {
    element.addEventListener('mousedown', this.boundOnMouseDown);
  }

  private onMouseDown(e: MouseEvent) {
    document.body.addEventListener('mousemove', this.boundOnMouseMove);
    document.body.addEventListener('mouseup', this.boundOnMouseUp);
    this.pendingMouseDownEvent = e;
    // Prevent interactions with other DragGestureHandlers and event listeners
    e.stopPropagation();
  }

  // We don't start the drag gesture on mouse down, instead we wait until
  // the mouse has moved at least 1px. This prevents accidental drags that
  // were meant to be clicks.
  private startDragGesture(e: MouseEvent) {
    this.clientRect = this.element.getBoundingClientRect() as DOMRect;
    this.onDragStarted(
        e.clientX - this.clientRect.left, e.clientY - this.clientRect.top);
  }

  private onMouseMove(e: MouseEvent) {
    if (e.buttons === 0) {
      return this.onMouseUp(e);
    }
    if (this.pendingMouseDownEvent &&
        (Math.abs(e.clientX - this.pendingMouseDownEvent.clientX) > 1 ||
         Math.abs(e.clientY - this.pendingMouseDownEvent.clientY) > 1)) {
      this.startDragGesture(this.pendingMouseDownEvent);
      this.pendingMouseDownEvent = undefined;
    }
    if (!this.pendingMouseDownEvent) {
      this.onDrag(
          e.clientX - this.clientRect!.left, e.clientY - this.clientRect!.top);
    }
    e.stopPropagation();
  }

  private onMouseUp(e: MouseEvent) {
    document.body.removeEventListener('mousemove', this.boundOnMouseMove);
    document.body.removeEventListener('mouseup', this.boundOnMouseUp);
    if (!this.pendingMouseDownEvent) {
      this.onDragFinished();
    }
    e.stopPropagation();
  }
}
