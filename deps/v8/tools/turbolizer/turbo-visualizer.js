// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

class Snapper {

  constructor(resizer) {
    let snapper = this;
    snapper.resizer = resizer;
    snapper.sourceExpand = d3.select("#" + SOURCE_EXPAND_ID);
    snapper.sourceCollapse = d3.select("#" + SOURCE_COLLAPSE_ID);
    snapper.disassemblyExpand = d3.select("#" + DISASSEMBLY_EXPAND_ID);
    snapper.disassemblyCollapse = d3.select("#" + DISASSEMBLY_COLLAPSE_ID);

    d3.select("#source-collapse").on("click", function () {
      resizer.snapper.toggleSourceExpanded();
    });
    d3.select("#disassembly-collapse").on("click", function () {
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

  toggleSourceExpanded() {
    this.setSourceExpanded(!this.sourceExpand.classed("invisible"));
  }

  sourceExpandUpdate(newState) {
    this.setLastExpandedState("source", newState);
    this.sourceExpand.classed("invisible", newState);
    this.sourceCollapse.classed("invisible", !newState);
  }

  setSourceExpanded(newState) {
    if (this.sourceExpand.classed("invisible") === newState) return;
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
    this.setDisassemblyExpanded(!this.disassemblyExpand.classed("invisible"));
  }

  disassemblyExpandUpdate(newState) {
    this.setLastExpandedState("disassembly", newState);
    this.disassemblyExpand.classed("invisible", newState);
    this.disassemblyCollapse.classed("invisible", !newState);
  }

  setDisassemblyExpanded(newState) {
    if (this.disassemblyExpand.classed("invisible") === newState) return;
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
  constructor(panes_updated_callback, dead_width) {
    let resizer = this;
    resizer.snapper = new Snapper(resizer)
    resizer.panes_updated_callback = panes_updated_callback;
    resizer.dead_width = dead_width
    resizer.client_width = d3.select("body").node().getBoundingClientRect().width;
    resizer.left = d3.select("#" + SOURCE_PANE_ID);
    resizer.middle = d3.select("#" + INTERMEDIATE_PANE_ID);
    resizer.right = d3.select("#" + GENERATED_PANE_ID);
    resizer.resizer_left = d3.select('.resizer-left');
    resizer.resizer_right = d3.select('.resizer-right');
    resizer.sep_left = resizer.client_width / 3;
    resizer.sep_right = resizer.client_width / 3 * 2;
    resizer.sep_left_snap = 0;
    resizer.sep_right_snap = 0;
    // Offset to prevent resizers from sliding slightly over one another.
    resizer.sep_width_offset = 7;

    let dragResizeLeft = d3.behavior.drag()
      .on('drag', function () {
        let x = d3.mouse(this.parentElement)[0];
        resizer.sep_left = Math.min(Math.max(0, x), resizer.sep_right - resizer.sep_width_offset);
        resizer.updatePanes();
      })
      .on('dragstart', function () {
        resizer.resizer_left.classed("dragged", true);
        let x = d3.mouse(this.parentElement)[0];
        if (x > dead_width) {
          resizer.sep_left_snap = resizer.sep_left;
        }
      })
      .on('dragend', function () {
        resizer.resizer_left.classed("dragged", false);
      });
    resizer.resizer_left.call(dragResizeLeft);

    let dragResizeRight = d3.behavior.drag()
      .on('drag', function () {
        let x = d3.mouse(this.parentElement)[0];
        resizer.sep_right = Math.max(resizer.sep_left + resizer.sep_width_offset, Math.min(x, resizer.client_width));
        resizer.updatePanes();
      })
      .on('dragstart', function () {
        resizer.resizer_right.classed("dragged", true);
        let x = d3.mouse(this.parentElement)[0];
        if (x < (resizer.client_width - dead_width)) {
          resizer.sep_right_snap = resizer.sep_right;
        }
      })
      .on('dragend', function () {
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
    this.left.style('width', this.sep_left + 'px');
    this.middle.style('width', (this.sep_right - this.sep_left) + 'px');
    this.right.style('width', (this.client_width - this.sep_right) + 'px');
    this.resizer_left.style('left', this.sep_left + 'px');
    this.resizer_right.style('right', (this.client_width - this.sep_right - 1) + 'px');

    this.snapper.panesUpated();
    this.panes_updated_callback();
  }

  updateWidths() {
    this.client_width = d3.select("body").node().getBoundingClientRect().width;
    this.sep_right = Math.min(this.sep_right, this.client_width);
    this.sep_left = Math.min(Math.max(0, this.sep_left), this.sep_right);
  }
}

document.onload = (function (d3) {
  "use strict";
  var svg = null;
  var graph = null;
  var schedule = null;
  var currentPhaseView = null;
  var disassemblyView = null;
  var sourceViews = [];
  var selectionBroker = null;
  var sourceResolver = null;
  let resizer = new Resizer(panesUpdatedCallback, 100);

  function panesUpdatedCallback() {
    if (graph) graph.fitGraphViewToWindow();
  }

  function hideCurrentPhase() {
    var rememberedSelection = null;
    if (currentPhaseView != null) {
      rememberedSelection = currentPhaseView.detachSelection();
      currentPhaseView.hide();
      currentPhaseView = null;
    }
    return rememberedSelection;
  }

  function displayPhaseView(view, data) {
    var rememberedSelection = hideCurrentPhase();
    view.show(data, rememberedSelection);
    d3.select("#middle").classed("scrollable", view.isScrollable());
    currentPhaseView = view;
  }

  function displayPhase(phase) {
    if (phase.type == 'graph') {
      displayPhaseView(graph, phase.data);
    } else if (phase.type == 'schedule') {
      displayPhaseView(schedule, phase);
    }
  }

  function loadFile(txtRes) {
    // If the JSON isn't properly terminated, assume compiler crashed and
    // add best-guess empty termination
    if (txtRes[txtRes.length - 2] == ',') {
      txtRes += '{"name":"disassembly","type":"disassembly","data":""}]}';
    }
    try {
      sourceViews.forEach((sv) => sv.hide());
      hideCurrentPhase();
      graph = null;
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
          sourceText: jsonObj.source
        };
      }

      sourceResolver.setInlinings(jsonObj.inlinings);
      sourceResolver.setSources(jsonObj.sources, fnc)
      sourceResolver.setNodePositionMap(jsonObj.nodePositions);
      sourceResolver.parsePhases(jsonObj.phases);

      let sourceView = new CodeView(SOURCE_PANE_ID, selectionBroker, sourceResolver, fnc, CodeView.MAIN_SOURCE);
      sourceView.show(null, null);
      sourceViews.push(sourceView);

      sourceResolver.forEachSource((source) => {
        let sourceView = new CodeView(SOURCE_PANE_ID, selectionBroker, sourceResolver, source, CodeView.INLINED_SOURCE);
        sourceView.show(null, null);
        sourceViews.push(sourceView);
      });

      disassemblyView = new DisassemblyView(GENERATED_PANE_ID, selectionBroker);
      disassemblyView.initializeCode(fnc.sourceText);
      if (sourceResolver.disassemblyPhase) {
        disassemblyView.initializePerfProfile(jsonObj.eventCounts);
        disassemblyView.show(sourceResolver.disassemblyPhase.data, null);
      }

      var selectMenu = document.getElementById('display-selector');
      selectMenu.innerHTML = '';
      sourceResolver.forEachPhase((phase) => {
        var optionElement = document.createElement("option");
        optionElement.text = phase.name;
        selectMenu.add(optionElement, null);
      });
      selectMenu.onchange = function (item) {
        window.sessionStorage.setItem("lastSelectedPhase", selectMenu.selectedIndex);
        displayPhase(sourceResolver.getPhase(selectMenu.selectedIndex));
      }

      const initialPhaseIndex = sourceResolver.repairPhaseId(+window.sessionStorage.getItem("lastSelectedPhase"));
      selectMenu.selectedIndex = initialPhaseIndex;

      function displayPhaseByName(phaseName) {
        const phaseId = sourceResolver.getPhaseIdByName(phaseName);
        selectMenu.selectedIndex = phaseId - 1;
        displayPhase(sourceResolver.getPhase(phaseId));
      }

      graph = new GraphView(d3, INTERMEDIATE_PANE_ID, selectionBroker, displayPhaseByName);
      schedule = new ScheduleView(INTERMEDIATE_PANE_ID, selectionBroker);

      displayPhase(sourceResolver.getPhase(initialPhaseIndex));

      d3.select("#search-input").attr("value", window.sessionStorage.getItem("lastSearch") || "");
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
    d3.select("#upload-helper").on("change", function () {
      if (window.File && window.FileReader && window.FileList) {
        var uploadFile = this.files && this.files[0];
        var filereader = new window.FileReader();
        filereader.onload = function (e) {
          var txtRes = e.target.result;
          loadFile(txtRes);
        };
        if (uploadFile)
          filereader.readAsText(uploadFile);
      } else {
        alert("Can't load graph");
      }
    });
  }

  initializeUploadHandlers();

  function handleSearch(e) {
    if (currentPhaseView) {
      currentPhaseView.searchInputAction(currentPhaseView, this)
    }
  }

  d3.select("#search-input").on("keyup", handleSearch);

  resizer.snapper.setSourceExpanded(resizer.snapper.getLastExpandedState("source", true));
  resizer.snapper.setDisassemblyExpanded(resizer.snapper.getLastExpandedState("disassembly", false));

  resizer.updatePanes();

})(window.d3);
