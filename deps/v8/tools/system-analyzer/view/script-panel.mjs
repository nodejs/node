// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import {App} from '../index.mjs'

import {SelectionEvent, SelectRelatedEvent, ToolTipEvent} from './events.mjs';
import {arrayEquals, CollapsableElement, CSSColor, defer, delay, DOM, formatBytes, gradientStopsFromGroups, groupBy, LazyTable} from './helper.mjs';

// A source mapping proxy for source maps that don't have CORS headers.
// TODO(leszeks): Make this configurable.
const sourceMapFetchPrefix = 'http://localhost:8080/';

DOM.defineCustomElement('view/script-panel',
                        (templateText) =>
                            class SourcePanel extends CollapsableElement {
  _selectedSourcePositions = [];
  _sourcePositionsToMarkNodesPromise = defer();
  _scripts = [];
  _script;

  showToolTipEntriesHandler = this.handleShowToolTipEntries.bind(this);

  constructor() {
    super(templateText);
    this.scriptDropdown.addEventListener(
        'change', e => this._handleSelectScript(e));
    this.$('#selectedRelatedButton').onclick =
        this._handleSelectRelated.bind(this);
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
    script.ensureSourceMapCalculated(sourceMapFetchPrefix);
    this._sourcePositionsToMarkNodesPromise = defer();
    this._selectedSourcePositions =
        this._selectedSourcePositions.filter(each => each.script === script);
    this.requestUpdate();
  }

  set focusedSourcePositions(sourcePositions) {
    this.selectedSourcePositions = sourcePositions;
  }

  set selectedSourcePositions(sourcePositions) {
    if (arrayEquals(this._selectedSourcePositions, sourcePositions)) {
      this._focusSelectedMarkers(0);
    } else {
      this._selectedSourcePositions = sourcePositions;
      // TODO: highlight multiple scripts
      this.script = sourcePositions[0]?.script;
      this._focusSelectedMarkers(100);
    }
  }

  set scripts(scripts) {
    this._scripts = scripts;
    this._initializeScriptDropdown();
  }

  get scriptDropdown() {
    return this.$('#script-dropdown');
  }

  _update() {
    this._renderSourcePanel();
    this._updateScriptDropdownSelection();
  }

  _initializeScriptDropdown() {
    this._scripts.sort((a, b) => a.name?.localeCompare(b.name) ?? 0);
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
    const script = this._script;
    if (script) {
      await delay(1);
      if (script != this._script) return;
      const builder = new LineBuilder(this, this._script);
      scriptNode = await builder.createScriptNode(this._script.startLine);
      if (script != this._script) return;
      this._sourcePositionsToMarkNodesPromise.resolve(
          builder.sourcePositionToMarkers);
    } else {
      scriptNode = DOM.div();
      this._selectedMarkNodes = undefined;
      this._sourcePositionsToMarkNodesPromise.resolve(new Map());
    }
    const oldScriptNode = this.script.childNodes[1];
    this.script.replaceChild(scriptNode, oldScriptNode);
  }

  async _focusSelectedMarkers(delay_ms) {
    if (delay_ms) await delay(delay_ms);
    const sourcePositionsToMarkNodes =
        await this._sourcePositionsToMarkNodesPromise;
    // Remove all marked nodes.
    for (let markNode of sourcePositionsToMarkNodes.values()) {
      markNode.className = '';
    }
    for (let sourcePosition of this._selectedSourcePositions) {
      if (sourcePosition.script !== this._script) continue;
      sourcePositionsToMarkNodes.get(sourcePosition).className = 'marked';
    }
    this._scrollToFirstSourcePosition(sourcePositionsToMarkNodes)
  }

  _scrollToFirstSourcePosition(sourcePositionsToMarkNodes) {
    const sourcePosition = this._selectedSourcePositions.find(
        each => each.script === this._script);
    if (!sourcePosition) return;
    const markNode = sourcePositionsToMarkNodes.get(sourcePosition);
    markNode.scrollIntoView(
        {behavior: 'smooth', block: 'center', inline: 'center'});
  }

  _handleSelectScript(e) {
    const option =
        this.scriptDropdown.options[this.scriptDropdown.selectedIndex];
    this.script = option.script;
  }

  _handleSelectRelated(e) {
    if (!this._script) return;
    this.dispatchEvent(new SelectRelatedEvent(this._script));
  }

  setSelectedSourcePositionInternal(sourcePosition) {
    this._selectedSourcePositions = [sourcePosition];
    console.assert(sourcePosition.script === this._script);
  }

  handleSourcePositionClick(e) {
    const sourcePosition = e.target.sourcePosition;
    this.setSelectedSourcePositionInternal(sourcePosition);
    this.dispatchEvent(new SelectRelatedEvent(sourcePosition));
  }

  handleSourcePositionMouseOver(e) {
    const sourcePosition = e.target.sourcePosition;
    const entries = sourcePosition.entries;
    const toolTipContent = DOM.div();
    toolTipContent.appendChild(
        new ToolTipTableBuilder(this, entries).tableNode);

    let sourceMapContent;
    switch (this._script.sourceMapState) {
      case 'loaded': {
        const originalPosition = sourcePosition.originalPosition;
        if (originalPosition.source === null) {
          sourceMapContent =
              DOM.element('i', {textContent: 'no source mapping for location'});
        } else {
          sourceMapContent = DOM.element('a', {
            href: `${originalPosition.source}`,
            target: '_blank',
            textContent: `${originalPosition.source}:${originalPosition.line}:${
                originalPosition.column}`
          });
        }
        break;
      }
      case 'loading':
        sourceMapContent =
            DOM.element('i', {textContent: 'source map still loading...'});
        break;
      case 'failed':
        sourceMapContent =
            DOM.element('i', {textContent: 'source map failed to load'});
        break;
      case 'none':
        sourceMapContent = DOM.element('i', {textContent: 'no source map'});
        break;
      default:
        break;
    }
    toolTipContent.appendChild(sourceMapContent);
    this.dispatchEvent(new ToolTipEvent(toolTipContent, e.target, e.ctrlKey));
  }

  handleShowToolTipEntries(event) {
    let entries = event.currentTarget.data;
    const sourcePosition = entries[0].sourcePosition;
    // Add a source position entry so the current position stays focused.
    this.setSelectedSourcePositionInternal(sourcePosition);
    entries = entries.concat(this._selectedSourcePositions);
    this.dispatchEvent(new SelectionEvent(entries));
  }
});

class ToolTipTableBuilder {
  constructor(scriptPanel, entries) {
    this._scriptPanel = scriptPanel;
    this.tableNode = DOM.table();
    const tr = DOM.tr();
    tr.appendChild(DOM.td('Type'));
    tr.appendChild(DOM.td('Subtype'));
    tr.appendChild(DOM.td('Count'));
    this.tableNode.appendChild(document.createElement('thead')).appendChild(tr);
    groupBy(entries, each => each.constructor, true).forEach(group => {
      this.addRow(group.key.name, 'all', entries, false)
      groupBy(group.entries, each => each.type, true).forEach(group => {
        this.addRow('', group.key, group.entries, false)
      })
    })
  }

  addRow(name, subtypeName, entries) {
    const tr = DOM.tr();
    tr.appendChild(DOM.td(name));
    tr.appendChild(DOM.td(subtypeName));
    tr.appendChild(DOM.td(entries.length));
    const button =
        DOM.button('🔎', this._scriptPanel.showToolTipEntriesHandler);
    button.title = `Show all ${entries.length} ${name || subtypeName} entries.`
    button.data = entries;
    tr.appendChild(DOM.td(button));
    this.tableNode.appendChild(tr);
  }
}

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
    return this._index >= this._entries.length;
  }

  _next() {
    this._index++;
  }
}

function* lineIterator(source, startLine) {
  let current = 0;
  let line = startLine;
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
  static _colorMap = (function() {
    const map = new Map();
    let i = 0;
    for (let type of App.allEventTypes) {
      map.set(type, CSSColor.at(i++));
    }
    return map;
  })();
  static get colorMap() {
    return this._colorMap;
  }

  _script;
  _clickHandler;
  _mouseoverHandler;
  _sourcePositionToMarkers = new Map();

  constructor(panel, script) {
    this._script = script;
    this._clickHandler = panel.handleSourcePositionClick.bind(panel);
    this._mouseoverHandler = panel.handleSourcePositionMouseOver.bind(panel);
  }

  get sourcePositionToMarkers() {
    return this._sourcePositionToMarkers;
  }

  async createScriptNode(startLine) {
    const scriptNode = DOM.div('scriptNode');

    // TODO: sort on script finalization.
    this._script.sourcePositions.sort((a, b) => {
      if (a.line === b.line) return a.column - b.column;
      return a.line - b.line;
    });

    const sourcePositionsIterator =
        new SourcePositionIterator(this._script.sourcePositions);
    scriptNode.style.counterReset = `sourceLineCounter ${startLine - 1}`;
    for (let [lineIndex, line] of lineIterator(
             this._script.source, startLine)) {
      scriptNode.appendChild(
          this._createLineNode(sourcePositionsIterator, lineIndex, line));
    }
    if (this._script.sourcePositions.length !=
        this._sourcePositionToMarkers.size) {
      console.error('Not all SourcePositions were processed.');
    }
    return scriptNode;
  }

  _createLineNode(sourcePositionsIterator, lineIndex, line) {
    const lineNode = DOM.span();
    let columnIndex = 0;
    for (const sourcePosition of sourcePositionsIterator.forLine(lineIndex)) {
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
    marker.onmouseover = this._mouseoverHandler;

    const entries = sourcePosition.entries;
    const groups = groupBy(entries, entry => entry.constructor);
    if (groups.length > 1) {
      const stops = gradientStopsFromGroups(
          entries.length, '%', groups, type => LineBuilder.colorMap.get(type));
      marker.style.backgroundImage = `linear-gradient(0deg,${stops.join(',')})`
    } else {
      marker.style.backgroundColor = LineBuilder.colorMap.get(groups[0].key)
    }

    return marker;
  }
}
