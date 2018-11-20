// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import * as C from "./constants.js"
import {SourceResolver} from "./source-resolver.js"
import {SelectionBroker} from "./selection-broker.js"
import {DisassemblyView} from "./disassembly-view.js"
import {GraphMultiView} from "./graphmultiview.js"
import {CodeMode, CodeView} from "./code-view.js"
import * as d3 from "d3"

class Snapper {
  resizer: Resizer;
  sourceExpand: HTMLElement;
  sourceCollapse: HTMLElement;
  disassemblyExpand: HTMLElement;
  disassemblyCollapse: HTMLElement;

  constructor(resizer: Resizer) {
    const snapper = this;
    snapper.resizer = resizer;
    snapper.sourceExpand = document.getElementById(C.SOURCE_EXPAND_ID);
    snapper.sourceCollapse = document.getElementById(C.SOURCE_COLLAPSE_ID);
    snapper.disassemblyExpand = document.getElementById(C.DISASSEMBLY_EXPAND_ID);
    snapper.disassemblyCollapse = document.getElementById(C.DISASSEMBLY_COLLAPSE_ID);

    document.getElementById("source-collapse").addEventListener("click", function () {
      resizer.snapper.toggleSourceExpanded();
    });
    document.getElementById("disassembly-collapse").addEventListener("click", function () {
      resizer.snapper.toggleDisassemblyExpanded();
    });
  }

  getLastExpandedState(type, default_state) {
    var state = window.sessionStorage.getItem("expandedState-" + type);
    if (state === null) return default_state;
    return state === 'true';
  }

  setLastExpandedState(type, state) {
    window.sessionStorage.setItem("expandedState-" + type, state);
  }

  toggleSourceExpanded(): void {
    this.setSourceExpanded(!this.sourceExpand.classList.contains("invisible"));
  }

  sourceExpandUpdate(newState: boolean) {
    this.setLastExpandedState("source", newState);
    this.sourceExpand.classList.toggle("invisible", newState);
    this.sourceCollapse.classList.toggle("invisible", !newState);
  }

  setSourceExpanded(newState) {
    if (this.sourceExpand.classList.contains("invisible") === newState) return;
    this.sourceExpandUpdate(newState);
    let resizer = this.resizer;
    if (newState) {
      resizer.sep_left = resizer.sep_left_snap;
      resizer.sep_left_snap = 0;
    } else {
      resizer.sep_left_snap = resizer.sep_left;
      resizer.sep_left = 0;
    }
    resizer.updatePanes();
  }

  toggleDisassemblyExpanded() {
    this.setDisassemblyExpanded(!this.disassemblyExpand.classList.contains("invisible"));
  }

  disassemblyExpandUpdate(newState) {
    this.setLastExpandedState("disassembly", newState);
    this.disassemblyExpand.classList.toggle("invisible", newState);
    this.disassemblyCollapse.classList.toggle("invisible", !newState);
  }

  setDisassemblyExpanded(newState) {
    if (this.disassemblyExpand.classList.contains("invisible") === newState) return;
    this.disassemblyExpandUpdate(newState);
    let resizer = this.resizer;
    if (newState) {
      resizer.sep_right = resizer.sep_right_snap;
      resizer.sep_right_snap = resizer.client_width;
    } else {
      resizer.sep_right_snap = resizer.sep_right;
      resizer.sep_right = resizer.client_width;
    }
    resizer.updatePanes();
  }

  panesUpated() {
    this.sourceExpandUpdate(this.resizer.sep_left > this.resizer.dead_width);
    this.disassemblyExpandUpdate(this.resizer.sep_right <
      (this.resizer.client_width - this.resizer.dead_width));
  }
}

class Resizer {
  snapper: Snapper;
  dead_width: number;
  client_width: number;
  left: HTMLElement;
  right: HTMLElement;
  middle: HTMLElement;
  sep_left: number;
  sep_right: number;
  sep_left_snap: number;
  sep_right_snap: number;
  sep_width_offset: number;
  panes_updated_callback: () => void;
  resizer_right: d3.Selection<HTMLDivElement, any, any, any>;
  resizer_left: d3.Selection<HTMLDivElement, any, any, any>;

  constructor(panes_updated_callback: () => void, dead_width: number) {
    let resizer = this;
    resizer.snapper = new Snapper(resizer)
    resizer.panes_updated_callback = panes_updated_callback;
    resizer.dead_width = dead_width
    resizer.client_width = document.body.getBoundingClientRect().width;
    resizer.left = document.getElementById(C.SOURCE_PANE_ID);
    resizer.middle = document.getElementById(C.INTERMEDIATE_PANE_ID);
    resizer.right = document.getElementById(C.GENERATED_PANE_ID);
    resizer.resizer_left = d3.select('.resizer-left');
    resizer.resizer_right = d3.select('.resizer-right');
    resizer.sep_left = resizer.client_width / 3;
    resizer.sep_right = resizer.client_width / 3 * 2;
    resizer.sep_left_snap = 0;
    resizer.sep_right_snap = 0;
    // Offset to prevent resizers from sliding slightly over one another.
    resizer.sep_width_offset = 7;

    let dragResizeLeft = d3.drag()
      .on('drag', function () {
        let x = d3.mouse(this.parentElement)[0];
        resizer.sep_left = Math.min(Math.max(0, x), resizer.sep_right - resizer.sep_width_offset);
        resizer.updatePanes();
      })
      .on('start', function () {
        resizer.resizer_left.classed("dragged", true);
        let x = d3.mouse(this.parentElement)[0];
        if (x > dead_width) {
          resizer.sep_left_snap = resizer.sep_left;
        }
      })
      .on('end', function () {
        resizer.resizer_left.classed("dragged", false);
      });
    resizer.resizer_left.call(dragResizeLeft);

    let dragResizeRight = d3.drag()
      .on('drag', function () {
        let x = d3.mouse(this.parentElement)[0];
        resizer.sep_right = Math.max(resizer.sep_left + resizer.sep_width_offset, Math.min(x, resizer.client_width));
        resizer.updatePanes();
      })
      .on('start', function () {
        resizer.resizer_right.classed("dragged", true);
        let x = d3.mouse(this.parentElement)[0];
        if (x < (resizer.client_width - dead_width)) {
          resizer.sep_right_snap = resizer.sep_right;
        }
      })
      .on('end', function () {
        resizer.resizer_right.classed("dragged", false);
      });;
    resizer.resizer_right.call(dragResizeRight);
    window.onresize = function () {
      resizer.updateWidths();
      resizer.updatePanes();
    };
  }

  updatePanes() {
    let left_snapped = this.sep_left === 0;
    let right_snapped = this.sep_right >= this.client_width - 1;
    this.resizer_left.classed("snapped", left_snapped);
    this.resizer_right.classed("snapped", right_snapped);
    this.left.style.width = this.sep_left + 'px';
    this.middle.style.width = (this.sep_right - this.sep_left) + 'px';
    this.right.style.width = (this.client_width - this.sep_right) + 'px';
    this.resizer_left.style('left', this.sep_left + 'px');
    this.resizer_right.style('right', (this.client_width - this.sep_right - 1) + 'px');

    this.snapper.panesUpated();
    this.panes_updated_callback();
  }

  updateWidths() {
    this.client_width = document.body.getBoundingClientRect().width;
    this.sep_right = Math.min(this.sep_right, this.client_width);
    this.sep_left = Math.min(Math.max(0, this.sep_left), this.sep_right);
  }
}

window.onload = function () {
  var svg = null;
  var multiview = null;
  var disassemblyView = null;
  var sourceViews = [];
  var selectionBroker = null;
  var sourceResolver = null;
  let resizer = new Resizer(panesUpdatedCallback, 100);

  function panesUpdatedCallback() {
    if (multiview) multiview.onresize();
  }

  function loadFile(txtRes) {
    // If the JSON isn't properly terminated, assume compiler crashed and
    // add best-guess empty termination
    if (txtRes[txtRes.length - 2] == ',') {
      txtRes += '{"name":"disassembly","type":"disassembly","data":""}]}';
    }
    try {
      sourceViews.forEach((sv) => sv.hide());
      if (multiview) multiview.hide();
      multiview = null;
      if (disassemblyView) disassemblyView.hide();
      sourceViews = [];
      sourceResolver = new SourceResolver();
      selectionBroker = new SelectionBroker(sourceResolver);

      const jsonObj = JSON.parse(txtRes);

      let fnc = jsonObj.function;
      // Backwards compatibility.
      if (typeof fnc == 'string') {
        fnc = {
          functionName: fnc,
          sourceId: -1,
          startPosition: jsonObj.sourcePosition,
          endPosition: jsonObj.sourcePosition + jsonObj.source.length,
          sourceText: jsonObj.source,
          backwardsCompatibility: true
        };
      }

      sourceResolver.setInlinings(jsonObj.inlinings);
      sourceResolver.setSourceLineToBytecodePosition(jsonObj.sourceLineToBytecodePosition);
      sourceResolver.setSources(jsonObj.sources, fnc)
      sourceResolver.setNodePositionMap(jsonObj.nodePositions);
      sourceResolver.parsePhases(jsonObj.phases);

      let sourceView = new CodeView(C.SOURCE_PANE_ID, selectionBroker, sourceResolver, fnc, CodeMode.MAIN_SOURCE);
      sourceView.show(null, null);
      sourceViews.push(sourceView);

      sourceResolver.forEachSource((source) => {
        let sourceView = new CodeView(C.SOURCE_PANE_ID, selectionBroker, sourceResolver, source, CodeMode.INLINED_SOURCE);
        sourceView.show(null, null);
        sourceViews.push(sourceView);
      });

      disassemblyView = new DisassemblyView(C.GENERATED_PANE_ID, selectionBroker);
      disassemblyView.initializeCode(fnc.sourceText);
      if (sourceResolver.disassemblyPhase) {
        disassemblyView.initializePerfProfile(jsonObj.eventCounts);
        disassemblyView.show(sourceResolver.disassemblyPhase.data, null);
      }

      multiview = new GraphMultiView(C.INTERMEDIATE_PANE_ID, selectionBroker, sourceResolver);
      multiview.show(jsonObj);
    } catch (err) {
      if (window.confirm("Error: Exception during load of TurboFan JSON file:\n" +
        "error: " + err.message + "\nDo you want to clear session storage?")) {
        window.sessionStorage.clear();
      }
      return;
    }
  }

  function initializeUploadHandlers() {
    // The <input> form #upload-helper with type file can't be a picture.
    // We hence keep it hidden, and forward the click from the picture
    // button #upload.
    d3.select("#upload").on("click",
      () => document.getElementById("upload-helper").click());
    d3.select("#upload-helper").on("change", function (this: HTMLInputElement) {
      var uploadFile = this.files && this.files[0];
      var filereader = new FileReader();
      filereader.onload = function (e) {
        var txtRes = e.target.result;
        loadFile(txtRes);
      };
      if (uploadFile)
        filereader.readAsText(uploadFile);
    });
  }

  initializeUploadHandlers();


  resizer.snapper.setSourceExpanded(resizer.snapper.getLastExpandedState("source", true));
  resizer.snapper.setDisassemblyExpanded(resizer.snapper.getLastExpandedState("disassembly", false));

  resizer.updatePanes();

};
