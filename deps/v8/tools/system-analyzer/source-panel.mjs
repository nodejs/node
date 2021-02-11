// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import {FocusEvent, SelectionEvent} from './events.mjs';
import {delay, DOM, formatBytes, V8CustomElement} from './helper.mjs';
import {IcLogEntry} from './log/ic.mjs';
import {MapLogEntry} from './log/map.mjs';

DOM.defineCustomElement('source-panel',
                        (templateText) =>
                            class SourcePanel extends V8CustomElement {
  _selectedSourcePositions = [];
  _sourcePositionsToMarkNodes;
  _scripts = [];
  _script;
  constructor() {
    super(templateText);
    this.scriptDropdown.addEventListener(
        'change', e => this._handleSelectScript(e));
  }

  get script() {
    return this.$('#script');
  }

  get scriptNode() {
    return this.$('.scriptNode');
  }

  set script(script) {
    if (this._script === script) return;
    this._script = script;
    this._renderSourcePanel();
    this._updateScriptDropdownSelection();
  }

  set selectedSourcePositions(sourcePositions) {
    this._selectedSourcePositions = sourcePositions;
    // TODO: highlight multiple scripts
    this.script = sourcePositions[0]?.script;
    this._focusSelectedMarkers();
  }

  set data(scripts) {
    this._scripts = scripts;
    this._initializeScriptDropdown();
  }

  get scriptDropdown() {
    return this.$('#script-dropdown');
  }

  _initializeScriptDropdown() {
    this._scripts.sort((a, b) => a.name.localeCompare(b.name));
    let select = this.scriptDropdown;
    select.options.length = 0;
    for (const script of this._scripts) {
      const option = document.createElement('option');
      const size = formatBytes(script.source.length);
      option.text = `${script.name} (id=${script.id} size=${size})`;
      option.script = script;
      select.add(option);
    }
  }
  _updateScriptDropdownSelection() {
    this.scriptDropdown.selectedIndex =
        this._script ? this._scripts.indexOf(this._script) : -1;
  }

  async _renderSourcePanel() {
    let scriptNode;
    if (this._script) {
      await delay(1);
      const builder =
          new LineBuilder(this, this._script, this._selectedSourcePositions);
      scriptNode = builder.createScriptNode();
      this._sourcePositionsToMarkNodes = builder.sourcePositionToMarkers;
    } else {
      scriptNode = document.createElement('pre');
      this._selectedMarkNodes = undefined;
    }
    const oldScriptNode = this.script.childNodes[1];
    this.script.replaceChild(scriptNode, oldScriptNode);
  }

  async _focusSelectedMarkers() {
    await delay(100);
    // Remove all marked nodes.
    for (let markNode of this._sourcePositionsToMarkNodes.values()) {
      markNode.className = '';
    }
    for (let sourcePosition of this._selectedSourcePositions) {
      this._sourcePositionsToMarkNodes.get(sourcePosition).className = 'marked';
    }
    const sourcePosition = this._selectedSourcePositions[0];
    if (!sourcePosition) return;
    const markNode = this._sourcePositionsToMarkNodes.get(sourcePosition);
    markNode.scrollIntoView(
        {behavior: 'smooth', block: 'nearest', inline: 'center'});
  }

  _handleSelectScript(e) {
    const option =
        this.scriptDropdown.options[this.scriptDropdown.selectedIndex];
    this.script = option.script;
    this.selectLogEntries(this._script.entries());
  }

  handleSourcePositionClick(e) {
    this.selectLogEntries(e.target.sourcePosition.entries)
  }

  selectLogEntries(logEntries) {
    let icLogEntries = [];
    let mapLogEntries = [];
    for (const entry of logEntries) {
      if (entry instanceof MapLogEntry) {
        mapLogEntries.push(entry);
      } else if (entry instanceof IcLogEntry) {
        icLogEntries.push(entry);
      }
    }
    if (icLogEntries.length > 0) {
      this.dispatchEvent(new SelectionEvent(icLogEntries));
    }
    if (mapLogEntries.length > 0) {
      this.dispatchEvent(new SelectionEvent(mapLogEntries));
    }
  }
});

class SourcePositionIterator {
  _entries;
  _index = 0;
  constructor(sourcePositions) {
    this._entries = sourcePositions;
  }

  * forLine(lineIndex) {
    this._findStart(lineIndex);
    while (!this._done() && this._current().line === lineIndex) {
      yield this._current();
      this._next();
    }
  }

  _findStart(lineIndex) {
    while (!this._done() && this._current().line < lineIndex) {
      this._next();
    }
  }

  _current() {
    return this._entries[this._index];
  }

  _done() {
    return this._index + 1 >= this._entries.length;
  }

  _next() {
    this._index++;
  }
}

function* lineIterator(source) {
  let current = 0;
  let line = 1;
  while (current < source.length) {
    const next = source.indexOf('\n', current);
    if (next === -1) break;
    yield [line, source.substring(current, next)];
    line++;
    current = next + 1;
  }
  if (current < source.length) yield [line, source.substring(current)];
}

class LineBuilder {
  _script;
  _clickHandler;
  _sourcePositions;
  _selection;
  _sourcePositionToMarkers = new Map();

  constructor(panel, script, highlightPositions) {
    this._script = script;
    this._selection = new Set(highlightPositions);
    this._clickHandler = panel.handleSourcePositionClick.bind(panel);
    // TODO: sort on script finalization.
    script.sourcePositions.sort((a, b) => {
      if (a.line === b.line) return a.column - b.column;
      return a.line - b.line;
    });
    this._sourcePositions = new SourcePositionIterator(script.sourcePositions);
  }

  get sourcePositionToMarkers() {
    return this._sourcePositionToMarkers;
  }

  createScriptNode() {
    const scriptNode = document.createElement('pre');
    scriptNode.classList.add('scriptNode');
    for (let [lineIndex, line] of lineIterator(this._script.source)) {
      scriptNode.appendChild(this._createLineNode(lineIndex, line));
    }
    return scriptNode;
  }

  _createLineNode(lineIndex, line) {
    const lineNode = document.createElement('span');
    let columnIndex = 0;
    for (const sourcePosition of this._sourcePositions.forLine(lineIndex)) {
      const nextColumnIndex = sourcePosition.column - 1;
      lineNode.appendChild(document.createTextNode(
          line.substring(columnIndex, nextColumnIndex)));
      columnIndex = nextColumnIndex;

      lineNode.appendChild(
          this._createMarkerNode(line[columnIndex], sourcePosition));
      columnIndex++;
    }
    lineNode.appendChild(
        document.createTextNode(line.substring(columnIndex) + '\n'));
    return lineNode;
  }

  _createMarkerNode(text, sourcePosition) {
    const marker = document.createElement('mark');
    this._sourcePositionToMarkers.set(sourcePosition, marker);
    marker.textContent = text;
    marker.sourcePosition = sourcePosition;
    marker.onclick = this._clickHandler;
    return marker;
  }
}