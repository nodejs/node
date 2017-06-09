// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

document.onload = (function(d3){
  "use strict";
  var jsonObj;
  var sourceExpandClassList = document.getElementById(SOURCE_EXPAND_ID).classList;
  var sourceCollapseClassList = document.getElementById(SOURCE_COLLAPSE_ID).classList;
  var sourceExpanded = sourceCollapseClassList.contains(COLLAPSE_PANE_BUTTON_VISIBLE);
  var disassemblyExpandClassList = document.getElementById(DISASSEMBLY_EXPAND_ID).classList;
  var disassemblyCollapseClassList = document.getElementById(DISASSEMBLY_COLLAPSE_ID).classList;
  var disassemblyExpanded = disassemblyCollapseClassList.contains(COLLAPSE_PANE_BUTTON_VISIBLE);
  var svg  = null;
  var graph = null;
  var schedule = null;
  var empty = null;
  var currentPhaseView = null;
  var disassemblyView = null;
  var sourceView = null;
  var selectionBroker = null;

  function updatePanes() {
    if (sourceExpanded) {
      if (disassemblyExpanded) {
        d3.select("#" + SOURCE_PANE_ID).style(WIDTH, "30%");
        d3.select("#" + INTERMEDIATE_PANE_ID).style(WIDTH, "40%");
        d3.select("#" + GENERATED_PANE_ID).style(WIDTH, "30%");
      } else {
        d3.select("#" + SOURCE_PANE_ID).style(WIDTH, "50%");
        d3.select("#" + INTERMEDIATE_PANE_ID).style(WIDTH, "50%");
        d3.select("#" + GENERATED_PANE_ID).style(WIDTH, "0%");
      }
    } else {
      if (disassemblyExpanded) {
        d3.select("#" + SOURCE_PANE_ID).style(WIDTH, "0%");
        d3.select("#" + INTERMEDIATE_PANE_ID).style(WIDTH, "50%");
        d3.select("#" + GENERATED_PANE_ID).style(WIDTH, "50%");
      } else {
        d3.select("#" + SOURCE_PANE_ID).style(WIDTH, "0%");
        d3.select("#" + INTERMEDIATE_PANE_ID).style(WIDTH, "100%");
        d3.select("#" + GENERATED_PANE_ID).style(WIDTH, "0%");
      }
    }
  }

  function getLastExpandedState(type, default_state) {
    var state = window.sessionStorage.getItem("expandedState-"+type);
    if (state === null) return default_state;
    return state === 'true';
  }

  function setLastExpandedState(type, state) {
    window.sessionStorage.setItem("expandedState-"+type, state);
  }

  function toggleSourceExpanded() {
    setSourceExpanded(!sourceExpanded);
  }

  function setSourceExpanded(newState) {
    sourceExpanded = newState;
    setLastExpandedState("source", newState);
    updatePanes();
    if (newState) {
      sourceCollapseClassList.add(COLLAPSE_PANE_BUTTON_VISIBLE);
      sourceCollapseClassList.remove(COLLAPSE_PANE_BUTTON_INVISIBLE);
      sourceExpandClassList.add(COLLAPSE_PANE_BUTTON_INVISIBLE);
      sourceExpandClassList.remove(COLLAPSE_PANE_BUTTON_VISIBLE);
    } else {
      sourceCollapseClassList.add(COLLAPSE_PANE_BUTTON_INVISIBLE);
      sourceCollapseClassList.remove(COLLAPSE_PANE_BUTTON_VISIBLE);
      sourceExpandClassList.add(COLLAPSE_PANE_BUTTON_VISIBLE);
      sourceExpandClassList.remove(COLLAPSE_PANE_BUTTON_INVISIBLE);
    }
  }

  function toggleDisassemblyExpanded() {
    setDisassemblyExpanded(!disassemblyExpanded);
  }

  function setDisassemblyExpanded(newState) {
    disassemblyExpanded = newState;
    setLastExpandedState("disassembly", newState);
    updatePanes();
    if (newState) {
      disassemblyCollapseClassList.add(COLLAPSE_PANE_BUTTON_VISIBLE);
      disassemblyCollapseClassList.remove(COLLAPSE_PANE_BUTTON_INVISIBLE);
      disassemblyExpandClassList.add(COLLAPSE_PANE_BUTTON_INVISIBLE);
      disassemblyExpandClassList.remove(COLLAPSE_PANE_BUTTON_VISIBLE);
    } else {
      disassemblyCollapseClassList.add(COLLAPSE_PANE_BUTTON_INVISIBLE);
      disassemblyCollapseClassList.remove(COLLAPSE_PANE_BUTTON_VISIBLE);
      disassemblyExpandClassList.add(COLLAPSE_PANE_BUTTON_VISIBLE);
      disassemblyExpandClassList.remove(COLLAPSE_PANE_BUTTON_INVISIBLE);
    }
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
    disassemblyView.resizeToParent();
    sourceView.resizeToParent();

    d3.select("#left").classed("scrollable", true);
    d3.select("#right").classed("scrollable", true);
  }

  selectionBroker = new SelectionBroker();

  function initializeHandlers(g) {
    d3.select("#source-collapse").on("click", function(){
      toggleSourceExpanded(true);
      setTimeout(function(){
        g.fitGraphViewToWindow();
      }, 300);
    });
    d3.select("#disassembly-collapse").on("click", function(){
      toggleDisassemblyExpanded();
      setTimeout(function(){
        g.fitGraphViewToWindow();
      }, 300);
    });
    window.onresize = function(){
      fitPanesToParents();
    };
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

  setSourceExpanded(getLastExpandedState("source", true));
  setDisassemblyExpanded(getLastExpandedState("disassembly", false));

  displayPhaseView(empty, null);
  fitPanesToParents();
})(window.d3);
