// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import {App} from '../index.mjs'

import {SelectionEvent, SelectRelatedEvent, ToolTipEvent} from './events.mjs';
import {arrayEquals, CollapsableElement, CSSColor, defer, delay, DOM, formatBytes, gradientStopsFromGroups, groupBy, LazyTable} from './helper.mjs';

// A source mapping proxy for source maps that don't have CORS headers.
// TODO(leszeks): Make this configurable.
const sourceMapFetchPrefix = 'http://localhost:8080/';

DOM.defineCustomElement(
    'view/script-panel',
    (templateText) => class SourcePanel extends CollapsableElement {
      _selectedSourcePositions = [];
      _scripts = [];
      _script;
      _lineHeight = 0;
      _charWidth = 0;
      _charsPerLine = 0;
      _lineOffsets = [];
      _cumulativeHeights = [];
      _renderedStartLine = -1;
      _renderedEndLine = -1;
      _renderedStartChunk = 0;
      _renderedEndChunk = 0;
      _observer;

      showToolTipEntriesHandler = this.handleShowToolTipEntries.bind(this);

      constructor() {
        super(templateText);
        this.scriptDropdown.addEventListener(
            'change', e => this._handleSelectScript(e));
        this.$('#selectedRelatedButton').onclick =
            this._handleSelectRelated.bind(this);
        this.script.addEventListener('scroll', this._handleScroll.bind(this));
        this._observer = new ResizeObserver(this._handleResize.bind(this));
        this._observer.observe(this.script);
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
        this._lineOffsets = [];
        this._cumulativeHeights = [];
        this._renderedStartLine = -1;
        this._renderedEndLine = -1;
        this._renderedStartChunk = 0;
        this._renderedEndChunk = 0;
        if (script) {
          script.ensureSourceMapCalculated(sourceMapFetchPrefix);
          this._script.sourcePositions.sort((a, b) => {
            if (a.line === b.line) return a.column - b.column;
            return a.line - b.line;
          });
          this._calculateLineOffsets(script.source);
        }
        this._selectedSourcePositions = this._selectedSourcePositions.filter(
            each => each.script === script);
        this.requestUpdate();
      }

      set focusedSourcePositions(sourcePositions) {
        this.selectedSourcePositions = sourcePositions;
      }

      set selectedSourcePositions(sourcePositions) {
        if (arrayEquals(this._selectedSourcePositions, sourcePositions)) {
          this._scrollToFirstSourcePosition();
        } else {
          this._selectedSourcePositions = sourcePositions;
          // TODO: highlight multiple scripts
          this.script = sourcePositions[0]?.script;
          this._scrollToFirstSourcePosition();
          this._renderVisibleLines();
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

      _calculateLineOffsets(source) {
        this._lineOffsets = [0];
        for (let i = 0; i < source.length; i++) {
          if (source[i] === '\n') this._lineOffsets.push(i + 1);
        }
      }

      _measureFontMetrics() {
        const lineSpan = DOM.span('line');
        lineSpan.textContent = 'M';
        this.scriptNode.appendChild(lineSpan);
        this._lineHeight = lineSpan.offsetHeight;
        const computedStyle = getComputedStyle(lineSpan);
        const paddingLeft = parseFloat(computedStyle.paddingLeft);
        this.scriptNode.removeChild(lineSpan);

        const charSpan = DOM.span();
        charSpan.textContent = 'M';
        this.scriptNode.appendChild(charSpan);
        this._charWidth = charSpan.getBoundingClientRect().width;
        this.scriptNode.removeChild(charSpan);

        if (this._lineHeight === 0) this._lineHeight = 18;
        if (this._charWidth === 0) this._charWidth = 7;

        const clientWidth = this.script.clientWidth;
        const paddingWidth = paddingLeft || (3.5 * this._charWidth);

        this._charsPerLine = Math.max(
            1, Math.floor((clientWidth - paddingWidth) / this._charWidth) - 1);
      }

      _calculateLayout() {
        if (!this._script || this._charsPerLine === 0) return;

        this._cumulativeHeights = [0];
        let currentHeight = 0;
        const lineOffsets = this._lineOffsets;

        for (let i = 1; i < lineOffsets.length; i++) {
          const start = lineOffsets[i - 1];
          const end = lineOffsets[i] - 1;  // exclude newline
          const len = Math.max(0, end - start);

          const visualLines = Math.ceil(len / this._charsPerLine) || 1;
          currentHeight += visualLines * this._lineHeight;
          this._cumulativeHeights.push(currentHeight);
        }
        // Handle last line if no newline at EOF
        if (lineOffsets[lineOffsets.length - 1] <= this._script.source.length) {
          const start = lineOffsets[lineOffsets.length - 1];
          const end = this._script.source.length;
          if (end > start) {
            const len = end - start;
            const visualLines = Math.ceil(len / this._charsPerLine) || 1;
            currentHeight += visualLines * this._lineHeight;
            this._cumulativeHeights.push(currentHeight);
          }
        }
      }

      async _renderSourcePanel() {
        if (!this._script) {
          this.scriptNode.innerHTML = '';
          this.scriptNode.style.paddingTop = '0px';
          this.scriptNode.style.paddingBottom = '0px';
          return;
        }
        await delay(1);
        if (this._script !== this._script) return;

        this._measureFontMetrics();
        this._calculateLayout();
        this._renderVisibleLines();
      }

      _handleScroll() {
        this._renderVisibleLines();
      }

      _handleResize() {
        this._renderedStartLine = -1;
        this._renderedEndLine = -1;
        this._renderedStartChunk = 0;
        this._renderedEndChunk = 0;
        this._measureFontMetrics();
        this._calculateLayout();
        this._renderVisibleLines();
      }

      _findLineForOffset(offset) {
        let left = 0;
        let right = this._cumulativeHeights.length - 1;
        while (left <= right) {
          const mid = Math.floor((left + right) / 2);
          if (this._cumulativeHeights[mid] <= offset) {
            left = mid + 1;
          } else {
            right = mid - 1;
          }
        }
        return left;
      }

      _renderVisibleLines() {
        if (!this._script || this._lineHeight === 0 ||
            this._cumulativeHeights.length === 0)
          return;

        const container = this.script;
        const scrollTop = container.scrollTop;
        const viewportHeight = container.clientHeight;
        const scrollBottom = scrollTop + viewportHeight;

        // Find source line corresponding to scrollTop
        let startLine = this._findLineForOffset(scrollTop);
        const startLineHeightBefore =
            (startLine > 1) ? this._cumulativeHeights[startLine - 2] : 0;
        const startOffsetInLine =
            Math.max(0, scrollTop - startLineHeightBefore);
        let startChunk = Math.floor(startOffsetInLine / this._lineHeight);

        // Find source line corresponding to scrollTop + viewportHeight
        let endLine = this._findLineForOffset(scrollBottom);
        const endLineHeightBefore =
            (endLine > 1) ? this._cumulativeHeights[endLine - 2] : 0;
        const endOffsetInLine = Math.max(0, scrollBottom - endLineHeightBefore);
        let endChunk = Math.ceil(endOffsetInLine / this._lineHeight);

        const BUFFER_LINES = 5;

        // Expand buffer by lines if possible, falling back to chunk buffering
        // for large lines.
        if (startChunk > 10) {
          startChunk -= 10;
        } else {
          const originalStartLine = startLine;
          startLine = Math.max(1, startLine - BUFFER_LINES);
          if (startLine < originalStartLine) startChunk = 0;
        }

        if (endLine < this._lineOffsets.length) {
          const originalEndLine = endLine;
          endLine = Math.min(this._lineOffsets.length, endLine + BUFFER_LINES);
          if (endLine > originalEndLine)
            endChunk = Infinity;
          else
            endChunk += 10;
        } else {
          endChunk += 10;
        }

        const builder = new LineBuilder(this, this._script, this._charsPerLine);

        if (this._renderedStartLine === -1 ||
            startLine > this._renderedEndLine ||
            endLine < this._renderedStartLine ||
            (startLine === this._renderedEndLine &&
             startChunk >=
                 this._renderedEndChunk) ||  // Gap between rendered and new
            (endLine === this._renderedStartLine &&
             endChunk <= this._renderedStartChunk)) {
          const fragment = builder.createScriptFragment(
              startLine, startChunk, endLine, endChunk, this._lineOffsets);

          this.scriptNode.innerHTML = '';
          this.scriptNode.appendChild(fragment);
        } else {
          // Remove from top
          while (this.scriptNode.firstElementChild) {
            const child = this.scriptNode.firstElementChild;
            const childLine = parseInt(child.dataset.line, 10);
            const childChunk = parseInt(child.dataset.chunk, 10);

            if (childLine < startLine ||
                (childLine === startLine && childChunk < startChunk)) {
              this.scriptNode.removeChild(child);
            } else {
              break;
            }
          }

          // Remove from bottom
          while (this.scriptNode.lastElementChild) {
            const child = this.scriptNode.lastElementChild;
            const childLine = parseInt(child.dataset.line, 10);
            const childChunk = parseInt(child.dataset.chunk, 10);

            if (childLine > endLine ||
                (childLine === endLine && childChunk >= endChunk)) {
              this.scriptNode.removeChild(child);
            } else {
              break;
            }
          }

          // Prepend new top lines/chunks
          if (startLine < this._renderedStartLine ||
              (startLine === this._renderedStartLine &&
               startChunk < this._renderedStartChunk)) {
            const gapEndLine = this._renderedStartLine;
            const gapEndChunk = this._renderedStartChunk;

            const fragment = builder.createScriptFragment(
                startLine, startChunk, gapEndLine, gapEndChunk,
                this._lineOffsets);
            this.scriptNode.insertBefore(
                fragment, this.scriptNode.firstElementChild);
          }

          // Append new bottom lines/chunks
          if (endLine > this._renderedEndLine ||
              (endLine === this._renderedEndLine &&
               endChunk > this._renderedEndChunk)) {
            const gapStartLine = this._renderedEndLine;
            const gapStartChunk = this._renderedEndChunk;

            const fragment = builder.createScriptFragment(
                gapStartLine, gapStartChunk, endLine, endChunk,
                this._lineOffsets);
            this.scriptNode.appendChild(fragment);
          }
        }

        this._renderedStartLine = startLine;
        this._renderedEndLine = endLine;
        this._renderedStartChunk = startChunk;
        this._renderedEndChunk = endChunk;

        // Padding Top calculation
        const heightBeforeStartLine =
            (startLine > 1) ? this._cumulativeHeights[startLine - 2] : 0;
        const paddingTop =
            heightBeforeStartLine + startChunk * this._lineHeight;

        // Padding Bottom calculation
        const totalHeight =
            this._cumulativeHeights[this._cumulativeHeights.length - 1];

        let renderedBottom;
        if (endChunk === Infinity) {
          renderedBottom = this._cumulativeHeights[endLine - 1];
        } else {
          const heightBeforeEndLine =
              (endLine > 1) ? this._cumulativeHeights[endLine - 2] : 0;
          renderedBottom = heightBeforeEndLine + endChunk * this._lineHeight;
        }

        const paddingBottom = Math.max(0, totalHeight - renderedBottom);

        this.scriptNode.style.paddingTop = `${paddingTop}px`;
        this.scriptNode.style.paddingBottom = `${paddingBottom}px`;
        this.scriptNode.style.counterReset =
            `sourceLineCounter ${startChunk > 0 ? startLine : startLine - 1}`;

        this._updateHighlights();
      }

      _updateHighlights() {
        const marked = this.scriptNode.querySelectorAll('.marked');
        for (const node of marked) {
          node.classList.remove('marked');
        }

        const marks = this.scriptNode.querySelectorAll('mark');
        for (const mark of marks) {
          if (this._selectedSourcePositions.includes(mark.sourcePosition)) {
            mark.classList.add('marked');
          }
        }
      }

      _scrollToFirstSourcePosition() {
        const sourcePosition = this._selectedSourcePositions.find(
            each => each.script === this._script);
        if (!sourcePosition) return;

        // Approximate scroll position based on line number.
        // We could be more precise with _cumulativeHeights if we knew which
        // line index corresponds to sourcePosition.line sourcePosition.line is
        // 1-based source line index.
        const lineIndex = sourcePosition.line;
        if (lineIndex > 0 && lineIndex <= this._cumulativeHeights.length) {
          const targetOffset = this._cumulativeHeights[lineIndex - 1];
          const targetScrollTop = targetOffset - (this.script.clientHeight / 2);
          this.script.scrollTo({top: targetScrollTop, behavior: 'smooth'});
        }
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
      }

      handleSourcePositionClick(e) {
        const sourcePosition = e.target.sourcePosition;
        this.setSelectedSourcePositionInternal(sourcePosition);
        this.dispatchEvent(new SelectRelatedEvent(sourcePosition));
        this._renderVisibleLines();
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
              sourceMapContent = DOM.element(
                  'i', {textContent: 'no source mapping for location'});
            } else {
              sourceMapContent = DOM.element('a', {
                href: `${originalPosition.source}`,
                target: '_blank',
                textContent: `${originalPosition.source}:${
                    originalPosition.line}:${originalPosition.column}`
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
        this.dispatchEvent(
            new ToolTipEvent(toolTipContent, e.target, e.ctrlKey));
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

  seekToLine(lineIndex) {
    let left = 0;
    let right = this._entries.length - 1;
    let result = -1;

    while (left <= right) {
      const mid = Math.floor((left + right) / 2);
      if (this._entries[mid].line >= lineIndex) {
        result = mid;
        right = mid - 1;
      } else {
        left = mid + 1;
      }
    }

    if (result !== -1) {
      this._index = result;
      // Backup to find first entry for this line
      while (this._index > 0 &&
             this._entries[this._index - 1].line === lineIndex) {
        this._index--;
      }
    } else {
      this._index = this._entries.length;
    }
  }

  * forLine(lineIndex) {
    while (!this._done() && this._current().line === lineIndex) {
      yield this._current();
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
  _charsPerLine;

  constructor(panel, script, charsPerLine) {
    this._script = script;
    this._clickHandler = panel.handleSourcePositionClick.bind(panel);
    this._mouseoverHandler = panel.handleSourcePositionMouseOver.bind(panel);
    this._charsPerLine = charsPerLine;
  }

  get sourcePositionToMarkers() {
    return this._sourcePositionToMarkers;
  }

  createScriptFragment(startLine, startChunk, endLine, endChunk, lineOffsets) {
    const fragment = new DocumentFragment();
    const sourcePositionsIterator =
        new SourcePositionIterator(this._script.sourcePositions);
    sourcePositionsIterator.seekToLine(startLine);

    const source = this._script.source;

    // Use inclusive range for lines, as endLine might be partially rendered.
    // If endChunk is 0, endLine is effectively not rendered.
    let effectiveEndLine = endLine;
    if (endChunk === 0) effectiveEndLine--;

    for (let lineIndex = startLine; lineIndex <= effectiveEndLine;
         lineIndex++) {
      const start = lineOffsets[lineIndex - 1];
      const end = (lineIndex < lineOffsets.length) ?
          lineOffsets[lineIndex] - 1 :
          source.length;

      let lineText = '';
      if (start < source.length) {
        lineText = source.substring(start, end);
      }

      const currentStartChunk = (lineIndex === startLine) ? startChunk : 0;
      const currentEndChunk = (lineIndex === endLine) ? endChunk : Infinity;

      this._appendVisualLines(
          fragment, sourcePositionsIterator, lineIndex, lineText,
          currentStartChunk, currentEndChunk);
    }
    return fragment;
  }

  _appendVisualLines(
      fragment, sourcePositionsIterator, lineIndex, line, startChunk = 0,
      endChunk = Infinity) {
    const markers = [];
    for (const sourcePosition of sourcePositionsIterator.forLine(lineIndex)) {
      markers.push(sourcePosition);
    }

    let currentMarkerIndex = 0;
    let currentColumn = 0;
    let remainingText = line;
    let isFirstVisualLine = true;
    let chunkIndex = 0;

    // If empty line
    if (remainingText.length === 0) {
      // If startChunk > 0, we skip the only chunk, so render nothing.
      if (startChunk > 0) return;
      if (chunkIndex >= endChunk) return;

      const lineNode = DOM.span(
          isFirstVisualLine ? 'line line-start' : 'line line-continuation');
      lineNode.dataset.line = lineIndex;
      lineNode.dataset.chunk = chunkIndex;
      lineNode.appendChild(document.createTextNode('\n'));
      fragment.appendChild(lineNode);
      return;
    }

    // Fast forward if startChunk > 0
    if (startChunk > 0) {
      const skipChars = startChunk * this._charsPerLine;
      if (skipChars < remainingText.length) {
        // Advance text
        remainingText = remainingText.substring(skipChars);
        currentColumn += skipChars;
        chunkIndex = startChunk;
        isFirstVisualLine =
            (startChunk === 0);  // Always false if startChunk > 0

        // Advance markers
        // We need to skip markers that appear before currentColumn.
        // Markers are sorted by column.
        while (currentMarkerIndex < markers.length) {
          const marker = markers[currentMarkerIndex];
          // Marker column is 1-based.
          if (marker.column - 1 < currentColumn) {
            currentMarkerIndex++;
          } else {
            break;
          }
        }
      } else {
        // Skipped everything
        return;
      }
    }

    while (remainingText.length > 0 && chunkIndex < endChunk) {
      const chunkLength = Math.min(this._charsPerLine, remainingText.length);
      const chunkEndColumn = currentColumn + chunkLength;

      const lineNode = DOM.span(
          isFirstVisualLine ? 'line line-start' : 'line line-continuation');
      lineNode.dataset.line = lineIndex;
      lineNode.dataset.chunk = chunkIndex;
      isFirstVisualLine = false;

      let chunkText = remainingText.substring(0, chunkLength);
      let chunkColumn = 0;

      // Process markers within this chunk
      while (currentMarkerIndex < markers.length) {
        const marker = markers[currentMarkerIndex];
        const markerColumn = marker.column - 1;  // 0-based

        if (markerColumn >= chunkEndColumn) break;  // Marker is in next chunk

        // Marker starts in this chunk
        // Append text before marker
        const textBeforeMarkerLen =
            markerColumn - (currentColumn + chunkColumn);
        if (textBeforeMarkerLen > 0) {
          lineNode.appendChild(document.createTextNode(chunkText.substring(
              chunkColumn, chunkColumn + textBeforeMarkerLen)));
          chunkColumn += textBeforeMarkerLen;
        }

        // Append marker (assumes 1 char width as per original code)
        lineNode.appendChild(
            this._createMarkerNode(chunkText[chunkColumn], marker));
        chunkColumn++;
        currentMarkerIndex++;
      }

      // Append remaining text in this chunk
      if (chunkColumn < chunkText.length) {
        lineNode.appendChild(
            document.createTextNode(chunkText.substring(chunkColumn)));
      }

      fragment.appendChild(lineNode);

      remainingText = remainingText.substring(chunkLength);
      currentColumn += chunkLength;
      chunkIndex++;

      // Break if we processed all text and markers (safety check)
      if (remainingText.length === 0 && currentMarkerIndex >= markers.length)
        break;
    }
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
