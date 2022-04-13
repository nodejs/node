// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

export class CSSColor {
  static _cache = new Map();

  static get(name) {
    let color = this._cache.get(name);
    if (color !== undefined) return color;
    const style = getComputedStyle(document.body);
    color = style.getPropertyValue(`--${name}`);
    if (color === undefined) {
      throw new Error(`CSS color does not exist: ${name}`);
    }
    color = color.trim();
    this._cache.set(name, color);
    return color;
  }

  static reset() {
    this._cache.clear();
  }

  static get backgroundColor() {
    return this.get('background-color');
  }
  static get surfaceColor() {
    return this.get('surface-color');
  }
  static get primaryColor() {
    return this.get('primary-color');
  }
  static get secondaryColor() {
    return this.get('secondary-color');
  }
  static get onSurfaceColor() {
    return this.get('on-surface-color');
  }
  static get onBackgroundColor() {
    return this.get('on-background-color');
  }
  static get onPrimaryColor() {
    return this.get('on-primary-color');
  }
  static get onSecondaryColor() {
    return this.get('on-secondary-color');
  }
  static get defaultColor() {
    return this.get('default-color');
  }
  static get errorColor() {
    return this.get('error-color');
  }
  static get mapBackgroundColor() {
    return this.get('map-background-color');
  }
  static get timelineBackgroundColor() {
    return this.get('timeline-background-color');
  }
  static get red() {
    return this.get('red');
  }
  static get green() {
    return this.get('green');
  }
  static get yellow() {
    return this.get('yellow');
  }
  static get blue() {
    return this.get('blue');
  }

  static get orange() {
    return this.get('orange');
  }

  static get violet() {
    return this.get('violet');
  }

  static at(index) {
    return this.list[index % this.list.length];
  }

  static darken(hexColorString, amount = -50) {
    if (hexColorString[0] !== '#') {
      throw new Error(`Unsupported color: ${hexColorString}`);
    }
    let color = parseInt(hexColorString.substring(1), 16);
    let b = Math.min(Math.max((color & 0xFF) + amount, 0), 0xFF);
    let g = Math.min(Math.max(((color >> 8) & 0xFF) + amount, 0), 0xFF);
    let r = Math.min(Math.max(((color >> 16) & 0xFF) + amount, 0), 0xFF);
    color = (r << 16) + (g << 8) + b;
    return `#${color.toString(16).padStart(6, '0')}`;
  }

  static get list() {
    if (!this._colors) {
      this._colors = [
        this.green,
        this.violet,
        this.orange,
        this.yellow,
        this.primaryColor,
        this.red,
        this.blue,
        this.yellow,
        this.secondaryColor,
        this.darken(this.green),
        this.darken(this.violet),
        this.darken(this.orange),
        this.darken(this.yellow),
        this.darken(this.primaryColor),
        this.darken(this.red),
        this.darken(this.blue),
        this.darken(this.yellow),
        this.darken(this.secondaryColor),
      ];
    }
    return this._colors;
  }
}

import {DOM} from '../../js/web-api-helper.mjs';

const SVGNamespace = 'http://www.w3.org/2000/svg';
export class SVG {
  static element(type, classes) {
    const node = document.createElementNS(SVGNamespace, type);
    if (classes !== undefined) DOM.addClasses(node, classes);
    return node;
  }

  static svg(classes) {
    return this.element('svg', classes);
  }

  static rect(classes) {
    return this.element('rect', classes);
  }

  static g(classes) {
    return this.element('g', classes);
  }
}

export function $(id) {
  return document.querySelector(id)
}

import {V8CustomElement} from '../../js/web-api-helper.mjs'

export class CollapsableElement extends V8CustomElement {
  constructor(templateText) {
    super(templateText);
    this._hasPendingUpdate = false;
    this._closer.onclick = _ => this._requestUpdateIfVisible();
  }

  get _closer() {
    return this.$('#closer');
  }

  get _contentIsVisible() {
    return !this._closer.checked;
  }

  hide() {
    if (this._contentIsVisible) {
      this._closer.checked = true;
      this._requestUpdateIfVisible();
    }
  }

  show() {
    if (!this._contentIsVisible) {
      this._closer.checked = false;
      this._requestUpdateIfVisible();
    }
    this.scrollIntoView({behavior: 'smooth', block: 'center'});
  }

  requestUpdate(useAnimation = false) {
    // A pending update will be resolved later, no need to try again.
    if (this._hasPendingUpdate) return;
    this._hasPendingUpdate = true;
    this._requestUpdateIfVisible(useAnimation);
  }

  _requestUpdateIfVisible(useAnimation = true) {
    if (!this._contentIsVisible) return;
    return super.requestUpdate(useAnimation);
  }

  forceUpdate() {
    this._hasPendingUpdate = false;
    super.forceUpdate();
  }
}

export class ExpandableText {
  constructor(node, string, limit = 200) {
    this._node = node;
    this._string = string;
    this._delta = limit / 2;
    this._start = 0;
    this._end = string.length;
    this._button = this._createExpandButton();
    this.expand();
  }

  _createExpandButton() {
    const button = DOM.element('button');
    button.innerText = '...';
    button.onclick = (e) => {
      e.stopImmediatePropagation();
      this.expand(e.shiftKey);
    };
    button.title = 'Expand text. Use SHIFT-click to show all.'
    return button;
  }

  expand(showAll = false) {
    DOM.removeAllChildren(this._node);
    this._start = this._start + this._delta;
    this._end = this._end - this._delta;
    if (this._start >= this._end || showAll) {
      this._node.innerText = this._string;
      this._button.onclick = undefined;
      return;
    }
    this._node.appendChild(DOM.text(this._string.substring(0, this._start)));
    this._node.appendChild(this._button);
    this._node.appendChild(
        DOM.text(this._string.substring(this._end, this._string.length)));
  }
}

export class Chunked {
  constructor(iterable, limit) {
    this._iterator = iterable[Symbol.iterator]();
    this._limit = limit;
  }

  * next(limit = undefined) {
    for (let i = 0; i < (limit ?? this._limit); i++) {
      const {value, done} = this._iterator.next();
      if (done) {
        this._iterator = undefined;
        return;
      };
      yield value;
    }
  }

  get hasMore() {
    return this._iterator !== undefined;
  }
}

export class LazyTable {
  constructor(table, rowData, rowElementCreator, limit = 100) {
    this._table = table;
    this._chunkedRowData = new Chunked(rowData, limit);
    this._rowElementCreator = rowElementCreator;
    if (table.tBodies.length == 0) {
      table.appendChild(DOM.tbody());
    } else {
      table.replaceChild(DOM.tbody(), table.tBodies[0]);
    }
    if (!table.tFoot) this._addFooter();
    table.tFoot.addEventListener('click', this._clickHandler);
    this._addMoreRows();
  }

  _addFooter() {
    const td = DOM.td();
    td.setAttribute('colspan', 100);
    for (let addCount of [10, 100, 250, 500]) {
      const button = DOM.element('button');
      button.innerText = `+${addCount}`;
      button.onclick = (e) => this._addMoreRows(addCount);
      td.appendChild(button);
    }
    this._table.appendChild(DOM.element('tfoot'))
        .appendChild(DOM.tr())
        .appendChild(td);
  }

  _addMoreRows(count = undefined) {
    const fragment = new DocumentFragment();
    for (let row of this._chunkedRowData.next(count)) {
      const tr = this._rowElementCreator(row);
      fragment.appendChild(tr);
    }
    this._table.tBodies[0].appendChild(fragment);
    if (!this._chunkedRowData.hasMore) {
      DOM.removeAllChildren(this._table.tFoot);
    }
  }
}

export function gradientStopsFromGroups(
    totalLength, maxHeight, groups, colorFn) {
  const kMaxHeight = maxHeight === '%' ? 100 : maxHeight;
  const kUnit = maxHeight === '%' ? '%' : 'px';
  let increment = 0;
  let lastHeight = 0.0;
  const stops = [];
  for (let group of groups) {
    const color = colorFn(group.key);
    increment += group.length;
    const height = (increment / totalLength * kMaxHeight) | 0;
    stops.push(`${color} ${lastHeight}${kUnit} ${height}${kUnit}`)
    lastHeight = height;
  }
  return stops;
}

export * from '../helper.mjs';
export * from '../../js/web-api-helper.mjs'
