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

    d3.select("#source-collapse").on("click", function(){
      resizer.snapper.toggleSourceExpanded();
    });
    d3.select("#disassembly-collapse").on("click", function(){
      resizer.snapper.toggleDisassemblyExpanded();
    });
  }

  getLastExpandedState(type, default_state) {
    var state = window.sessionStorage.getItem("expandedState-"+type);
    if (state === null) return default_state;
    return state === 'true';
  }

  setLastExpandedState(type, state) {
    window.sessionStorage.setItem("expandedState-"+type, state);
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
    console.log(newState)
    if (this.disassemblyExpand.classed("invisible") === newState) return;
    this.disassemblyExpandUpdate(newState);
    let resizer = this.resizer;
    if (newState) {
      resizer.sep_right = resizer.sep_right_snap;
      resizer.sep_right_snap = resizer.client_width;
      console.log("set expand")
    } else {
      resizer.sep_right_snap = resizer.sep_right;
      resizer.sep_right = resizer.client_width;
      console.log("set collapse")
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
    resizer.sep_left = resizer.client_width/3;
    resizer.sep_right = resizer.client_width/3*2;
    resizer.sep_left_snap = 0;
    resizer.sep_right_snap = 0;
    // Offset to prevent resizers from sliding slightly over one another.
    resizer.sep_width_offset = 7;

    let dragResizeLeft = d3.behavior.drag()
      .on('drag', function() {
        let x = d3.mouse(this.parentElement)[0];
        resizer.sep_left = Math.min(Math.max(0,x), resizer.sep_right-resizer.sep_width_offset);
        resizer.updatePanes();
      })
      .on('dragstart', function() {
        resizer.resizer_left.classed("dragged", true);
        let x = d3.mouse(this.parentElement)[0];
        if (x > dead_width) {
          resizer.sep_left_snap = resizer.sep_left;
        }
      })
      .on('dragend', function() {
        resizer.resizer_left.classed("dragged", false);
      });
    resizer.resizer_left.call(dragResizeLeft);

    let dragResizeRight = d3.behavior.drag()
      .on('drag', function() {
        let x = d3.mouse(this.parentElement)[0];
        resizer.sep_right = Math.max(resizer.sep_left+resizer.sep_width_offset, Math.min(x, resizer.client_width));
        resizer.updatePanes();
      })
      .on('dragstart', function() {
        resizer.resizer_right.classed("dragged", true);
        let x = d3.mouse(this.parentElement)[0];
        if (x < (resizer.client_width-dead_width)) {
          resizer.sep_right_snap = resizer.sep_right;
        }
      })
      .on('dragend', function() {
        resizer.resizer_right.classed("dragged", false);
      });;
    resizer.resizer_right.call(dragResizeRight);
    window.onresize = function(){
      resizer.updateWidths();
      /*fitPanesToParents();*/
      resizer.updatePanes();
    };
  }

  updatePanes() {
    let left_snapped = this.sep_left === 0;
    let right_snapped = this.sep_right >= this.client_width - 1;
    this.resizer_left.classed("snapped", left_snapped);
    this.resizer_right.classed("snapped", right_snapped);
    this.left.style('width', this.sep_left + 'px');
    this.middle.style('width', (this.sep_right-this.sep_left) + 'px');
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

document.onload = (function(d3){
  "use strict";
  var jsonObj;
  var svg  = null;
  var graph = null;
  var schedule = null;
  var empty = null;
  var currentPhaseView = null;
  var disassemblyView = null;
  var sourceView = null;
  var selectionBroker = null;
  let resizer = new Resizer(panesUpdatedCallback, 100);

  function panesUpdatedCallback() {
    graph.fitGraphViewToWindow();
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
      displayPhaseView(schedule, phase.data);
    } else {
      displayPhaseView(empty, null);
    }
  }

  function fitPanesToParents() {
    d3.select("#left").classed("scrollable", false)
    d3.select("#right").classed("scrollable", false);

    graph.fitGraphViewToWindow();

    d3.select("#left").classed("scrollable", true);
    d3.select("#right").classed("scrollable", true);
  }

  selectionBroker = new SelectionBroker();

  function initializeHandlers(g) {
    d3.select("#hidden-file-upload").on("change", function() {
      if (window.File && window.FileReader && window.FileList) {
        var uploadFile = this.files[0];
        var filereader = new window.FileReader();
        var consts = Node.consts;
        filereader.onload = function(){
          var txtRes = filereader.result;
          // If the JSON isn't properly terminated, assume compiler crashed and
          // add best-guess empty termination
          if (txtRes[txtRes.length-2] == ',') {
            txtRes += '{"name":"disassembly","type":"disassembly","data":""}]}';
          }
          try{
            jsonObj = JSON.parse(txtRes);

            hideCurrentPhase();

            selectionBroker.setNodePositionMap(jsonObj.nodePositions);

            sourceView.initializeCode(jsonObj.source, jsonObj.sourcePosition);
            disassemblyView.initializeCode(jsonObj.source);

            var selectMenu = document.getElementById('display-selector');
            var disassemblyPhase = null;
            selectMenu.innerHTML = '';
            for (var i = 0; i < jsonObj.phases.length; ++i) {
              var optionElement = document.createElement("option");
              optionElement.text = jsonObj.phases[i].name;
              if (optionElement.text == 'disassembly') {
                disassemblyPhase = jsonObj.phases[i];
              } else {
                selectMenu.add(optionElement, null);
              }
            }

            disassemblyView.initializePerfProfile(jsonObj.eventCounts);
            disassemblyView.show(disassemblyPhase.data, null);

            var initialPhaseIndex = +window.sessionStorage.getItem("lastSelectedPhase");
            if (!(initialPhaseIndex in jsonObj.phases)) {
              initialPhaseIndex = 0;
            }

            // We wish to show the remembered phase {lastSelectedPhase}, but
            // this will crash if the first view we switch to is a
            // ScheduleView. So we first switch to the first phase, which
            // should never be a ScheduleView.
            displayPhase(jsonObj.phases[0]);
            displayPhase(jsonObj.phases[initialPhaseIndex]);
            selectMenu.selectedIndex = initialPhaseIndex;

            selectMenu.onchange = function(item) {
              window.sessionStorage.setItem("lastSelectedPhase", selectMenu.selectedIndex);
              displayPhase(jsonObj.phases[selectMenu.selectedIndex]);
            }

            fitPanesToParents();

            d3.select("#search-input").attr("value", window.sessionStorage.getItem("lastSearch") || "");

          }
          catch(err) {
            window.console.log("caught exception, clearing session storage just in case");
            window.sessionStorage.clear(); // just in case
            window.console.log("showing error");
            window.alert("Invalid TurboFan JSON file\n" +
                         "error: " + err.message);
            return;
          }
        };
        filereader.readAsText(uploadFile);
      } else {
        alert("Can't load graph");
      }
    });
  }

  sourceView = new CodeView(SOURCE_PANE_ID, PR, "", 0, selectionBroker);
  disassemblyView = new DisassemblyView(DISASSEMBLY_PANE_ID, selectionBroker);
  graph = new GraphView(d3, GRAPH_PANE_ID, [], [], selectionBroker);
  schedule = new ScheduleView(SCHEDULE_PANE_ID, selectionBroker);
  empty = new EmptyView(EMPTY_PANE_ID, selectionBroker);

  initializeHandlers(graph);

  resizer.snapper.setSourceExpanded(resizer.snapper.getLastExpandedState("source", true));
  resizer.snapper.setDisassemblyExpanded(resizer.snapper.getLastExpandedState("disassembly", false));

  displayPhaseView(empty, null);
  fitPanesToParents();
  resizer.updatePanes();

})(window.d3);
