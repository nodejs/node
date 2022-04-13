// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

export class V8CustomElement extends HTMLElement {
  _updateTimeoutId;
  _updateCallback = this.forceUpdate.bind(this);

  constructor(templateText) {
    super();
    const shadowRoot = this.attachShadow({mode: 'open'});
    shadowRoot.innerHTML = templateText;
  }

  $(id) {
    return this.shadowRoot.querySelector(id);
  }

  querySelectorAll(query) {
    return this.shadowRoot.querySelectorAll(query);
  }

  requestUpdate(useAnimation = false) {
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

  forceUpdate() {
    this._update();
  }

  _update() {
    throw Error('Subclass responsibility');
  }
}

export class FileReader extends V8CustomElement {
  constructor(templateText) {
    super(templateText);
    this.addEventListener('click', (e) => this.handleClick(e));
    this.addEventListener('dragover', (e) => this.handleDragOver(e));
    this.addEventListener('drop', (e) => this.handleChange(e));
    this.$('#file').addEventListener('change', (e) => this.handleChange(e));
    this.$('#fileReader')
        .addEventListener('keydown', (e) => this.handleKeyEvent(e));
  }

  set error(message) {
    this._updateLabel(message);
    this.root.className = 'fail';
  }

  _updateLabel(text) {
    this.$('#label').innerText = text;
  }

  handleKeyEvent(event) {
    if (event.key == 'Enter') this.handleClick(event);
  }

  handleClick(event) {
    this.$('#file').click();
  }

  handleChange(event) {
    // Used for drop and file change.
    event.preventDefault();
    this.dispatchEvent(
        new CustomEvent('fileuploadstart', {bubbles: true, composed: true}));
    const host = event.dataTransfer ? event.dataTransfer : event.target;
    this.readFile(host.files[0]);
  }

  handleDragOver(event) {
    event.preventDefault();
  }

  connectedCallback() {
    this.fileReader.focus();
  }

  get fileReader() {
    return this.$('#fileReader');
  }

  get root() {
    return this.$('#root');
  }

  readFile(file) {
    if (!file) {
      this.error = 'Failed to load file.';
      return;
    }
    this.fileReader.blur();
    this.root.className = 'loading';
    // Delay the loading a bit to allow for CSS animations to happen.
    window.requestAnimationFrame(() => this.asyncReadFile(file));
  }

  async asyncReadFile(file) {
    const decoder = globalThis.TextDecoderStream;
    if (decoder) {
      await this._streamFile(file, decoder);
    } else {
      await this._readFullFile(file);
    }
    this._updateLabel(`Finished loading '${file.name}'.`);
    this.dispatchEvent(
        new CustomEvent('fileuploadend', {bubbles: true, composed: true}));
    this.root.className = 'done';
  }

  async _readFullFile(file) {
    const text = await file.text();
    this._handleFileChunk(text);
  }

  async _streamFile(file, decoder) {
    const stream = file.stream().pipeThrough(new decoder());
    const reader = stream.getReader();
    let chunk, readerDone;
    do {
      const readResult = await reader.read();
      chunk = readResult.value;
      readerDone = readResult.done;
      if (chunk) this._handleFileChunk(chunk);
    } while (!readerDone);
  }

  _handleFileChunk(chunk) {
    this.dispatchEvent(new CustomEvent('fileuploadchunk', {
      bubbles: true,
      composed: true,
      detail: chunk,
    }));
  }
}

export class DOM {
  static element(type, options) {
    const node = document.createElement(type);
    if (options === undefined) return node;
    if (typeof options === 'string') {
      // Old behaviour: options = class string
      node.className = options;
    } else if (Array.isArray(options)) {
      // Old behaviour: options = class array
      DOM.addClasses(node, options);
    } else {
      // New behaviour: options = attribute dict
      for (const [key, value] of Object.entries(options)) {
        if (key == 'className') {
          node.className = value;
        } else if (key == 'classList') {
          DOM.addClasses(node, value);
        } else if (key == 'textContent') {
          node.textContent = value;
        } else if (key == 'children') {
          for (const child of value) {
            node.appendChild(child);
          }
        } else {
          node.setAttribute(key, value);
        }
      }
    }
    return node;
  }

  static addClasses(node, classes) {
    const classList = node.classList;
    if (typeof classes === 'string') {
      classList.add(classes);
    } else {
      for (let i = 0; i < classes.length; i++) {
        classList.add(classes[i]);
      }
    }
    return node;
  }

  static text(string) {
    return document.createTextNode(string);
  }

  static button(label, clickHandler) {
    const button = DOM.element('button');
    button.innerText = label;
    if (typeof clickHandler != 'function') {
      throw new Error(
          `DOM.button: Expected function but got clickHandler=${clickHandler}`);
    }
    button.onclick = clickHandler;
    return button;
  }

  static div(options) {
    return this.element('div', options);
  }

  static span(options) {
    return this.element('span', options);
  }

  static table(options) {
    return this.element('table', options);
  }

  static tbody(options) {
    return this.element('tbody', options);
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

  static defineCustomElement(
      path, nameOrGenerator, maybeGenerator = undefined) {
    let generator = nameOrGenerator;
    let name = nameOrGenerator;
    if (typeof nameOrGenerator == 'function') {
      console.assert(maybeGenerator === undefined);
      name = path.substring(path.lastIndexOf('/') + 1, path.length);
    } else {
      console.assert(typeof nameOrGenerator == 'string');
      generator = maybeGenerator;
    }
    path = path + '-template.html';
    fetch(path)
        .then(stream => stream.text())
        .then(
            templateText =>
                customElements.define(name, generator(templateText)));
  }
}
