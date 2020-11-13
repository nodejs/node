// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import { V8CustomElement, defineCustomElement } from "./helper.mjs";
import { SelectionEvent, FocusEvent } from "./events.mjs";
import { MapLogEvent } from "./log/map.mjs";
import { IcLogEvent } from "./log/ic.mjs";

defineCustomElement(
  "source-panel",
  (templateText) =>
    class SourcePanel extends V8CustomElement {
      #selectedSourcePositions;
      #scripts = [];
      #script;
      constructor() {
        super(templateText);
        this.scriptDropdown.addEventListener(
          'change', e => this.handleSelectScript(e));
      }
      get script() {
        return this.$('#script');
      }
      get scriptNode() {
        return this.$('.scriptNode');
      }
      set script(script) {
        this.#script = script;
        this.renderSourcePanel();
      }
      set selectedSourcePositions(sourcePositions) {
        this.#selectedSourcePositions = sourcePositions;
      }
      get selectedSourcePositions() {
        return this.#selectedSourcePositions;
      }
      set data(value) {
        this.#scripts = value;
        this.initializeScriptDropdown();
        this.script = this.#scripts[0];
      }
      get scriptDropdown() {
        return this.$("#script-dropdown");
      }
      initializeScriptDropdown() {
        this.#scripts.sort((a, b) => a.name.localeCompare(b.name));
        let select = this.scriptDropdown;
        select.options.length = 0;
        for (const script of this.#scripts) {
          const option = document.createElement("option");
          option.text = `${script.name} (id=${script.id})`;
          option.script = script;
          select.add(option);
        }
      }

      renderSourcePanel() {
        const builder = new LineBuilder(this, this.#script);
        const scriptNode = builder.createScriptNode();
        const oldScriptNode = this.script.childNodes[1];
        this.script.replaceChild(scriptNode, oldScriptNode);
      }

      handleSelectScript(e) {
        const option = this.scriptDropdown.options[this.scriptDropdown.selectedIndex];
        this.script = option.script;
      }

      handleSourcePositionClick(e) {
        let icLogEvents = [];
        let mapLogEvents = [];
        for (const entry of e.target.sourcePosition.entries) {
          if (entry instanceof MapLogEvent) {
            mapLogEvents.push(entry);
          } else if (entry instanceof IcLogEvent) {
            icLogEvents.push(entry);
          }
        }
        if (icLogEvents.length > 0 ) {
          this.dispatchEvent(new SelectionEvent(icLogEvents));
          this.dispatchEvent(new FocusEvent(icLogEvents[0]));
        }
        if (mapLogEvents.length > 0) {
          this.dispatchEvent(new SelectionEvent(mapLogEvents));
          this.dispatchEvent(new FocusEvent(mapLogEvents[0]));
        }
      }

    }
);


class SourcePositionIterator {
  #entries;
  #index = 0;
  constructor(sourcePositions) {
    this.#entries = sourcePositions;
  }

  *forLine(lineIndex) {
    while(!this.#done() && this.#current().line === lineIndex) {
      yield this.#current();
      this.#next();
    }
  }

  #current() {
    return this.#entries[this.#index];
  }

  #done() {
    return this.#index + 1 >= this.#entries.length;
  }

  #next() {
    this.#index++;
  }
}

function * lineIterator(source) {
  let current = 0;
  let line = 1;
  while(current < source.length) {
    const next = source.indexOf("\n", current);
    if (next === -1) break;
    yield [line, source.substring(current, next)];
    line++;
    current = next + 1;
  }
  if (current < source.length) yield [line, source.substring(current)];
}

class LineBuilder {
  #script
  #clickHandler
  #sourcePositions

  constructor(panel, script) {
    this.#script = script;
    this.#clickHandler = panel.handleSourcePositionClick.bind(panel);
    // TODO: sort on script finalization.
    script.sourcePositions.sort((a, b) => {
      if (a.line === b.line) return a.column - b.column;
      return a.line - b.line;
    })
    this.#sourcePositions
        = new SourcePositionIterator(script.sourcePositions);

  }

  createScriptNode() {
    const scriptNode = document.createElement("pre");
    scriptNode.classList.add('scriptNode');
    for (let [lineIndex, line] of lineIterator(this.#script.source)) {
      scriptNode.appendChild(this.#createLineNode(lineIndex, line));
    }
    return scriptNode;
  }

  #createLineNode(lineIndex, line) {
    const lineNode = document.createElement("span");
    let columnIndex  = 0;
    for (const sourcePosition of this.#sourcePositions.forLine(lineIndex)) {
      const nextColumnIndex = sourcePosition.column - 1;
      lineNode.appendChild(
          document.createTextNode(
            line.substring(columnIndex, nextColumnIndex)));
      columnIndex = nextColumnIndex;

      lineNode.appendChild(
          this.#createMarkerNode(line[columnIndex], sourcePosition));
      columnIndex++;
    }
    lineNode.appendChild(
        document.createTextNode(line.substring(columnIndex) + "\n"));
    return lineNode;
  }

  #createMarkerNode(text, sourcePosition) {
    const marker = document.createElement("mark");
    marker.classList.add('marked');
    marker.textContent = text;
    marker.sourcePosition = sourcePosition;
    marker.onclick = this.#clickHandler;
    return marker;
  }


}