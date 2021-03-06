// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import { PhaseView } from "../src/view";
import { anyToString, ViewElements, isIterable } from "../src/util";
import { MySelection } from "../src/selection";
import { SourceResolver } from "./source-resolver";
import { SelectionBroker } from "./selection-broker";
import { NodeSelectionHandler, BlockSelectionHandler } from "./selection-handler";

export abstract class TextView extends PhaseView {
  selectionHandler: NodeSelectionHandler;
  blockSelectionHandler: BlockSelectionHandler;
  selection: MySelection;
  blockSelection: MySelection;
  textListNode: HTMLUListElement;
  nodeIdToHtmlElementsMap: Map<string, Array<HTMLElement>>;
  blockIdToHtmlElementsMap: Map<string, Array<HTMLElement>>;
  blockIdtoNodeIds: Map<string, Array<string>>;
  nodeIdToBlockId: Array<string>;
  patterns: any;
  sourceResolver: SourceResolver;
  broker: SelectionBroker;

  constructor(id, broker) {
    super(id);
    const view = this;
    view.textListNode = view.divNode.getElementsByTagName('ul')[0];
    view.patterns = null;
    view.nodeIdToHtmlElementsMap = new Map();
    view.blockIdToHtmlElementsMap = new Map();
    view.blockIdtoNodeIds = new Map();
    view.nodeIdToBlockId = [];
    view.selection = new MySelection(anyToString);
    view.blockSelection = new MySelection(anyToString);
    view.broker = broker;
    view.sourceResolver = broker.sourceResolver;
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
    view.divNode.addEventListener('click', e => {
      if (!e.shiftKey) {
        view.selectionHandler.clear();
      }
      e.stopPropagation();
    });
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
    const elementsToSelect = view.divNode.querySelectorAll(`[data-pc-offset]`);
    for (const el of elementsToSelect) {
      el.classList.toggle("selected", false);
    }
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
    this.patterns = patterns;
  }

  clearText() {
    while (this.textListNode.firstChild) {
      this.textListNode.removeChild(this.textListNode.firstChild);
    }
  }

  createFragment(text, style) {
    const fragment = document.createElement("SPAN");

    if (typeof style.associateData == 'function') {
      if (style.associateData(text, fragment) === false) {
         return null;
      }
    } else {
      if (style.css != undefined) {
        const css = isIterable(style.css) ? style.css : [style.css];
        for (const cls of css) {
          fragment.classList.add(cls);
        }
      }
      fragment.innerText = text;
    }

    return fragment;
  }

  processLine(line) {
    const view = this;
    const result = [];
    let patternSet = 0;
    while (true) {
      const beforeLine = line;
      for (const pattern of view.patterns[patternSet]) {
        const matches = line.match(pattern[0]);
        if (matches != null) {
          if (matches[0] != '') {
            const style = pattern[1] != null ? pattern[1] : {};
            const text = matches[0];
            if (text != '') {
              const fragment = view.createFragment(matches[0], style);
              if (fragment !== null) result.push(fragment);
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
    const view = this;
    const textLines = text.split(/[\n]/);
    let lineNo = 0;
    for (const line of textLines) {
      const li = document.createElement("LI");
      li.className = "nolinenums";
      li.dataset.lineNo = "" + lineNo++;
      const fragments = view.processLine(line);
      for (const fragment of fragments) {
        li.appendChild(fragment);
      }
      view.textListNode.appendChild(li);
    }
  }

  initializeContent(data, rememberedSelection) {
    this.clearText();
    this.processText(data);
    this.show();
  }

  public onresize(): void {}
}
