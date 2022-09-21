// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

export function snakeToCamel(str: string): string {
  return str.toLowerCase().replace(/([-_][a-z])/g, group =>
    group
      .toUpperCase()
      .replace('-', '')
      .replace('_', ''));
}

export function camelize(obj: any): any {
  if (Array.isArray(obj)) {
    return obj.map(value => camelize(value));
  } else if (obj !== null && obj.constructor === Object) {
    return Object.keys(obj).reduce((result, key: string) => ({
        ...result,
        [snakeToCamel(key)]: camelize(obj[key])
      }), {},
    );
  }
  return obj;
}

export function sortUnique<T>(arr: Array<T>, comparator: (a: T, b: T) => number,
                              equals: (a: T, b: T) => boolean): Array<T> {
  if (arr.length == 0) return arr;
  arr = arr.sort(comparator);
  const uniqueArr = [arr[0]];
  for (let i = 1; i < arr.length; i++) {
    if (!equals(arr[i - 1], arr[i])) {
      uniqueArr.push(arr[i]);
    }
  }
  return uniqueArr;
}

// Partial application without binding the receiver
export function partial(func: any, ...arguments1: Array<any>) {
  return function (this: any, ...arguments2: Array<any>) {
    func.apply(this, [...arguments1, ...arguments2]);
  };
}

export function isIterable(obj: any): obj is Iterable<any> {
  return obj !== null && obj !== undefined
    && typeof obj !== "string" && typeof obj[Symbol.iterator] === "function";
}

export function alignUp(raw: number, multiple: number): number {
  return Math.floor((raw + multiple - 1) / multiple) * multiple;
}

export function measureText(text: string, coefficient: number = 1):
  { width: number, height: number } {
  const textMeasure = document.getElementById("text-measure");
  if (textMeasure instanceof SVGTSpanElement) {
    textMeasure.textContent = text;
    return {
      width: textMeasure.getBBox().width * coefficient,
      height: textMeasure.getBBox().height * coefficient,
    };
  }
  return { width: 0, height: 0 };
}

// Interpolate between the given start and end values by a fraction of val/max.
export function interpolate(val: number, max: number, start: number, end: number): number {
  return start + (end - start) * (val / max);
}

export function createElement(tag: string, cls: string, content?: string): HTMLElement {
  const el = document.createElement(tag);
  el.className = cls;
  if (content !== undefined) el.innerText = content;
  return el;
}

export function storageGetItem(key: string, defaultValue?: any, parse: boolean = true): any {
  let value = window.sessionStorage.getItem(key);
  if (parse) value = JSON.parse(value);
  return value === null ? defaultValue : value;
}

export function storageSetItem(key: string, value: any): void {
  window.sessionStorage.setItem(key, value);
}

export function storageSetIfIsNotExist(key: string, toSet: any): void {
  if (storageGetItem(key, null, false) === null) storageSetItem(key, toSet);
}

export function copyToClipboard(text: string): void {
  if (!text || text.length == 0) return;
  navigator.clipboard.writeText(text);
}

export function getNumericCssValue(varName: string): number {
  const propertyValue = getComputedStyle(document.body).getPropertyValue(varName);
  return parseFloat(propertyValue.match(/[+-]?\d+(\.\d+)?/g)[0]);
}
