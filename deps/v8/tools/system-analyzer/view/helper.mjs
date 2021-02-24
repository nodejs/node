// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

class CSSColor {
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

  static darken(hexColorString, amount = -40) {
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

class DOM {
  static element(type, classes) {
    const node = document.createElement(type);
    if (classes === undefined) return node;
    if (typeof classes === 'string') {
      node.className = classes;
    } else {
      classes.forEach(cls => node.classList.add(cls));
    }
    return node;
  }

  static text(string) {
    return document.createTextNode(string);
  }

  static div(classes) {
    return this.element('div', classes);
  }

  static span(classes) {
    return this.element('span', classes);
  }

  static table(classes) {
    return this.element('table', classes);
  }

  static tbody(classes) {
    return this.element('tbody', classes);
  }

  static td(textOrNode, className) {
    const node = this.element('td');
    if (typeof textOrNode === 'object') {
      node.appendChild(textOrNode);
    } else if (textOrNode) {
      node.innerText = textOrNode;
    }
    if (className) node.className = className;
    return node;
  }

  static tr(classes) {
    return this.element('tr', classes);
  }

  static removeAllChildren(node) {
    let range = document.createRange();
    range.selectNodeContents(node);
    range.deleteContents();
  }

  static defineCustomElement(path, generator) {
    let name = path.substring(path.lastIndexOf('/') + 1, path.length);
    path = path + '-template.html';
    fetch(path)
        .then(stream => stream.text())
        .then(
            templateText =>
                customElements.define(name, generator(templateText)));
  }
}

function $(id) {
  return document.querySelector(id)
}

class V8CustomElement extends HTMLElement {
  _updateTimeoutId;
  _updateCallback = this._update.bind(this);

  constructor(templateText) {
    super();
    const shadowRoot = this.attachShadow({mode: 'open'});
    shadowRoot.innerHTML = templateText;
    this._updateCallback = this._update.bind(this);
  }

  $(id) {
    return this.shadowRoot.querySelector(id);
  }

  querySelectorAll(query) {
    return this.shadowRoot.querySelectorAll(query);
  }

  update(useAnimation = false) {
    if (useAnimation) {
      window.cancelAnimationFrame(this._updateTimeoutId);
      this._updateTimeoutId =
          window.requestAnimationFrame(this._updateCallback);
    } else {
      // Use timeout tasks to asynchronously update the UI without blocking.
      clearTimeout(this._updateTimeoutId);
      const kDelayMs = 5;
      this._updateTimeoutId = setTimeout(this._updateCallback, kDelayMs);
    }
  }

  _update() {
    throw Error('Subclass responsibility');
  }
}

class Chunked {
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

class LazyTable {
  constructor(table, rowData, rowElementCreator, limit = 100) {
    this._table = table;
    this._chunkedRowData = new Chunked(rowData, limit);
    this._rowElementCreator = rowElementCreator;
    if (table.tBodies.length == 0) {
      table.appendChild(DOM.tbody());
    } else {
      table.replaceChild(DOM.tbody(), table.tBodies[0]);
    }
    if (!table.tFoot) {
      const td = table.appendChild(DOM.element('tfoot'))
                     .appendChild(DOM.tr())
                     .appendChild(DOM.td());
      for (let count of [10, 100]) {
        const button = DOM.element('button');
        button.innerText = `+${count}`;
        button.onclick = (e) => this._addMoreRows(count);
        td.appendChild(button);
      }
      td.setAttribute('colspan', 100);
    }
    table.tFoot.addEventListener('click', this._clickHandler);
    this._addMoreRows();
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
    increment += group.count;
    let height = (increment / totalLength * kMaxHeight) | 0;
    stops.push(`${color} ${lastHeight}${kUnit} ${height}${kUnit}`)
    lastHeight = height;
  }
  return stops;
}

export * from '../helper.mjs';
export {
  DOM,
  $,
  V8CustomElement,
  CSSColor,
  LazyTable,
};
