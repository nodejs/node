// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

export function anyToString(x: any): string {
  return "" + x;
}

function computeScrollTop(container, element) {
  const height = container.offsetHeight;
  const margin = Math.floor(height / 4);
  const pos = element.offsetTop;
  const currentScrollTop = container.scrollTop;
  if (pos < currentScrollTop + margin) {
    return Math.max(0, pos - margin);
  } else if (pos > (currentScrollTop + 3 * margin)) {
    return Math.max(0, pos - 3 * margin);
  }
  return pos;
}

export class ViewElements {
  container: HTMLElement;
  scrollTop: number;

  constructor(container: HTMLElement) {
    this.container = container;
    this.scrollTop = undefined;
  }

  consider(element, doConsider) {
    if (!doConsider) return;
    const newScrollTop = computeScrollTop(this.container, element);
    if (isNaN(newScrollTop)) {
      console.log("NOO");
    }
    if (this.scrollTop === undefined) {
      this.scrollTop = newScrollTop;
    } else {
      this.scrollTop = Math.min(this.scrollTop, newScrollTop);
    }
  }

  apply(doApply) {
    if (!doApply || this.scrollTop === undefined) return;
    this.container.scrollTop = this.scrollTop;
  }
}

export function sortUnique<T>(arr: Array<T>, f: (a: T, b: T) => number, equal: (a: T, b: T) => boolean) {
  if (arr.length == 0) return arr;
  arr = arr.sort(f);
  const ret = [arr[0]];
  for (let i = 1; i < arr.length; i++) {
    if (!equal(arr[i - 1], arr[i])) {
      ret.push(arr[i]);
    }
  }
  return ret;
}

// Partial application without binding the receiver
export function partial(f: any, ...arguments1: Array<any>) {
  return function (this: any, ...arguments2: Array<any>) {
    f.apply(this, [...arguments1, ...arguments2]);
  };
}

export function isIterable(obj: any): obj is Iterable<any> {
  return obj != null && obj != undefined
    && typeof obj != 'string' && typeof obj[Symbol.iterator] === 'function';
}

export function alignUp(raw: number, multiple: number): number {
  return Math.floor((raw + multiple - 1) / multiple) * multiple;
}

export function measureText(text: string) {
  const textMeasure = document.getElementById('text-measure');
  if (textMeasure instanceof SVGTSpanElement) {
    textMeasure.textContent = text;
    return {
      width: textMeasure.getBBox().width,
      height: textMeasure.getBBox().height,
    };
  }
  return { width: 0, height: 0 };
}

// Interpolate between the given start and end values by a fraction of val/max.
export function interpolate(val: number, max: number, start: number, end: number) {
  return start + (end - start) * (val / max);
}

export function createElement(tag: string, cls: string, content?: string) {
  const el = document.createElement(tag);
  el.className = cls;
  if (content != undefined) el.innerText = content;
  return el;
}
