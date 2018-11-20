// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {View} from "./view.js"
import {anyToString, ViewElements, isIterable} from "./util.js"
import {MySelection} from "./selection.js"

export abstract class TextView extends View {
  selectionHandler: NodeSelectionHandler;
  blockSelectionHandler: BlockSelectionHandler;
  nodeSelectionHandler: NodeSelectionHandler;
  selection: MySelection;
  blockSelection: MySelection;
  textListNode: HTMLUListElement;
  nodeIdToHtmlElementsMap: Map<string, Array<HTMLElement>>;
  blockIdToHtmlElementsMap: Map<string, Array<HTMLElement>>;
  blockIdtoNodeIds: Map<string, Array<string>>;
  nodeIdToBlockId: Array<string>;
  patterns: any;

  constructor(id, broker, patterns) {
    super(id);
    let view = this;
    view.textListNode = view.divNode.getElementsByTagName('ul')[0];
    view.patterns = patterns;
    view.nodeIdToHtmlElementsMap = new Map();
    view.blockIdToHtmlElementsMap = new Map();
    view.blockIdtoNodeIds = new Map();
    view.nodeIdToBlockId = [];
    view.selection = new MySelection(anyToString);
    view.blockSelection = new MySelection(anyToString);
    const selectionHandler = {
      clear: function () {
        view.selection.clear();
        view.updateSelection();
        broker.broadcastClear(selectionHandler);
      },
      select: function (nodeIds, selected) {
        view.selection.select(nodeIds, selected);
        view.updateSelection();
        broker.broadcastNodeSelect(selectionHandler, view.selection.selectedKeys(), selected);
      },
      brokeredNodeSelect: function (nodeIds, selected) {
        const firstSelect = view.blockSelection.isEmpty();
        view.selection.select(nodeIds, selected);
        view.updateSelection(firstSelect);
      },
      brokeredClear: function () {
        view.selection.clear();
        view.updateSelection();
      }
    };
    this.selectionHandler = selectionHandler;
    broker.addNodeHandler(selectionHandler);
    view.divNode.onmouseup = function (e) {
      if (!e.shiftKey) {
        view.selectionHandler.clear();
      }
    }
    const blockSelectionHandler = {
      clear: function () {
        view.blockSelection.clear();
        view.updateSelection();
        broker.broadcastClear(blockSelectionHandler);
      },
      select: function (blockIds, selected) {
        view.blockSelection.select(blockIds, selected);
        view.updateSelection();
        broker.broadcastBlockSelect(blockSelectionHandler, blockIds, selected);
      },
      brokeredBlockSelect: function (blockIds, selected) {
        const firstSelect = view.blockSelection.isEmpty();
        view.blockSelection.select(blockIds, selected);
        view.updateSelection(firstSelect);
      },
      brokeredClear: function () {
        view.blockSelection.clear();
        view.updateSelection();
      }
    };
    this.blockSelectionHandler = blockSelectionHandler;
    broker.addBlockHandler(blockSelectionHandler);
  }

  addHtmlElementForNodeId(anyNodeId: any, htmlElement: HTMLElement) {
    const nodeId = anyToString(anyNodeId);
    if (!this.nodeIdToHtmlElementsMap.has(nodeId)) {
      this.nodeIdToHtmlElementsMap.set(nodeId, []);
    }
    this.nodeIdToHtmlElementsMap.get(nodeId).push(htmlElement);
  }

  addHtmlElementForBlockId(anyBlockId, htmlElement) {
    const blockId = anyToString(anyBlockId);
    if (!this.blockIdToHtmlElementsMap.has(blockId)) {
      this.blockIdToHtmlElementsMap.set(blockId, []);
    }
    this.blockIdToHtmlElementsMap.get(blockId).push(htmlElement);
  }

  addNodeIdToBlockId(anyNodeId, anyBlockId) {
    const blockId = anyToString(anyBlockId);
    if (!this.blockIdtoNodeIds.has(blockId)) {
      this.blockIdtoNodeIds.set(blockId, []);
    }
    this.blockIdtoNodeIds.get(blockId).push(anyToString(anyNodeId));
    this.nodeIdToBlockId[anyNodeId] = blockId;
  }

  blockIdsForNodeIds(nodeIds) {
    const blockIds = [];
    for (const nodeId of nodeIds) {
      const blockId = this.nodeIdToBlockId[nodeId];
      if (blockId == undefined) continue;
      blockIds.push(blockId);
    }
    return blockIds;
  }

  updateSelection(scrollIntoView: boolean = false) {
    if (this.divNode.parentNode == null) return;
    const mkVisible = new ViewElements(this.divNode.parentNode as HTMLElement);
    const view = this;
    for (const [blockId, elements] of this.blockIdToHtmlElementsMap.entries()) {
      const isSelected = view.blockSelection.isSelected(blockId);
      for (const element of elements) {
        mkVisible.consider(element, isSelected);
        element.classList.toggle("selected", isSelected);
      }
    }
    for (const key of this.nodeIdToHtmlElementsMap.keys()) {
      for (const element of this.nodeIdToHtmlElementsMap.get(key)) {
        element.classList.toggle("selected", false);
      }
    }
    for (const nodeId of view.selection.selectedKeys()) {
      const elements = this.nodeIdToHtmlElementsMap.get(nodeId);
      if (!elements) continue;
      for (const element of elements) {
        mkVisible.consider(element, true);
        element.classList.toggle("selected", true);
      }
    }
    mkVisible.apply(scrollIntoView);
  }

  setPatterns(patterns) {
    let view = this;
    view.patterns = patterns;
  }

  clearText() {
    let view = this;
    while (view.textListNode.firstChild) {
      view.textListNode.removeChild(view.textListNode.firstChild);
    }
  }

  createFragment(text, style) {
    let view = this;
    let fragment = document.createElement("SPAN");

    if (style.blockId != undefined) {
      const blockId = style.blockId(text);
      if (blockId != undefined) {
        fragment.blockId = blockId;
        this.addHtmlElementForBlockId(blockId, fragment);
      }
    }

    if (typeof style.link == 'function') {
      fragment.classList.add('linkable-text');
      fragment.onmouseup = function (e) {
        e.stopPropagation();
        style.link(text)
      };
    }

    if (typeof style.nodeId == 'function') {
      const nodeId = style.nodeId(text);
      if (nodeId != undefined) {
        fragment.nodeId = nodeId;
        this.addHtmlElementForNodeId(nodeId, fragment);
      }
    }

    if (typeof style.assignBlockId === 'function') {
      fragment.blockId = style.assignBlockId();
      this.addNodeIdToBlockId(fragment.nodeId, fragment.blockId);
    }

    if (typeof style.linkHandler == 'function') {
      const handler = style.linkHandler(text, fragment)
      if (handler !== undefined) {
        fragment.classList.add('linkable-text');
        fragment.onmouseup = handler;
      }
    }

    if (style.css != undefined) {
      const css = isIterable(style.css) ? style.css : [style.css];
      for (const cls of css) {
        fragment.classList.add(cls);
      }
    }
    fragment.innerHTML = text;
    return fragment;
  }

  processLine(line) {
    let view = this;
    let result = [];
    let patternSet = 0;
    while (true) {
      let beforeLine = line;
      for (let pattern of view.patterns[patternSet]) {
        let matches = line.match(pattern[0]);
        if (matches != null) {
          if (matches[0] != '') {
            let style = pattern[1] != null ? pattern[1] : {};
            let text = matches[0];
            if (text != '') {
              let fragment = view.createFragment(matches[0], style);
              result.push(fragment);
            }
            line = line.substr(matches[0].length);
          }
          let nextPatternSet = patternSet;
          if (pattern.length > 2) {
            nextPatternSet = pattern[2];
          }
          if (line == "") {
            if (nextPatternSet != -1) {
              throw ("illegal parsing state in text-view in patternSet" + patternSet);
            }
            return result;
          }
          patternSet = nextPatternSet;
          break;
        }
      }
      if (beforeLine == line) {
        throw ("input not consumed in text-view in patternSet" + patternSet);
      }
    }
  }

  processText(text) {
    let view = this;
    let textLines = text.split(/[\n]/);
    let lineNo = 0;
    for (let line of textLines) {
      let li = document.createElement("LI");
      li.className = "nolinenums";
      li.dataset.lineNo = "" + lineNo++;
      let fragments = view.processLine(line);
      for (let fragment of fragments) {
        li.appendChild(fragment);
      }
      view.textListNode.appendChild(li);
    }
  }

  initializeContent(data, rememberedSelection) {
    let view = this;
    view.clearText();
    view.processText(data);
  }

  deleteContent() {
  }

  isScrollable() {
    return true;
  }
}
