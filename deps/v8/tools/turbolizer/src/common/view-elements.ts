// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

export class ViewElements {
  container: HTMLElement;
  scrollTop: number;

  constructor(container: HTMLElement) {
    this.container = container;
    this.scrollTop = undefined;
  }

  public consider(element: HTMLElement, doConsider: boolean): void {
    if (!doConsider) return;
    const newScrollTop = this.computeScrollTop(element);
    if (isNaN(newScrollTop)) {
      console.warn("New scroll top value is NaN");
    }
    if (this.scrollTop === undefined) {
      this.scrollTop = newScrollTop;
    } else {
      this.scrollTop = Math.min(this.scrollTop, newScrollTop);
    }
  }

  public apply(doApply: boolean): void {
    if (!doApply || this.scrollTop === undefined) return;
    this.container.scrollTop = this.scrollTop;
  }

  private computeScrollTop(element: HTMLElement): number {
    const height = this.container.offsetHeight;
    const margin = Math.floor(height / 4);
    const pos = element.offsetTop;
    const currentScrollTop = this.container.scrollTop;
    if (pos < currentScrollTop + margin) {
      return Math.max(0, pos - margin);
    } else if (pos > (currentScrollTop + 3 * margin)) {
      return Math.max(0, pos - 3 * margin);
    }
    return currentScrollTop;
  }
}
