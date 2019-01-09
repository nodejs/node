// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import * as d3 from "d3"
import { SourceResolver } from "../src/source-resolver"
import { SelectionBroker } from "../src/selection-broker"
import { DisassemblyView } from "../src/disassembly-view"
import { GraphMultiView } from "../src/graphmultiview"
import { CodeMode, CodeView } from "../src/code-view"
import { Tabs } from "../src/tabs"
import { Resizer } from "../src/resizer"
import * as C from "../src/constants"

window.onload = function () {
  var multiview = null;
  var disassemblyView = null;
  var sourceViews = [];
  var selectionBroker = null;
  var sourceResolver = null;
  let resizer = new Resizer(panesUpdatedCallback, 100);

  function panesUpdatedCallback() {
    if (multiview) multiview.onresize();
  }

  function loadFile(txtRes: string) {
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

      let fnc = null
      // Backwards compatibility.
      if (typeof jsonObj.function == 'string') {
        fnc = {
          functionName: fnc,
          sourceId: -1,
          startPosition: jsonObj.sourcePosition,
          endPosition: jsonObj.sourcePosition + jsonObj.source.length,
          sourceText: jsonObj.source,
          backwardsCompatibility: true
        };
      } else {
        fnc = Object.assign(jsonObj.function, { backwardsCompatibility: false });
      }

      sourceResolver.setInlinings(jsonObj.inlinings);
      sourceResolver.setSourceLineToBytecodePosition(jsonObj.sourceLineToBytecodePosition);
      sourceResolver.setSources(jsonObj.sources, fnc)
      sourceResolver.setNodePositionMap(jsonObj.nodePositions);
      sourceResolver.parsePhases(jsonObj.phases);

      const sourceTabsContainer = document.getElementById(C.SOURCE_PANE_ID);
      const sourceTabs = new Tabs(sourceTabsContainer);
      const [sourceTab, sourceContainer] = sourceTabs.addTabAndContent("Source")
      sourceContainer.classList.add("viewpane", "scrollable");
      sourceTabs.activateTab(sourceTab);
      sourceTabs.addTab("&#x2b;").classList.add("open-tab");
      let sourceView = new CodeView(sourceContainer, selectionBroker, sourceResolver, fnc, CodeMode.MAIN_SOURCE);
      sourceView.show(null, null);
      sourceViews.push(sourceView);

      sourceResolver.forEachSource((source) => {
        let sourceView = new CodeView(sourceContainer, selectionBroker, sourceResolver, source, CodeMode.INLINED_SOURCE);
        sourceView.show(null, null);
        sourceViews.push(sourceView);
      });

      const disassemblyTabsContainer = document.getElementById(C.GENERATED_PANE_ID);
      const disassemblyTabs = new Tabs(disassemblyTabsContainer);
      const [disassemblyTab, disassemblyContainer] = disassemblyTabs.addTabAndContent("Disassembly")
      disassemblyContainer.classList.add("viewpane", "scrollable");
      disassemblyTabs.activateTab(disassemblyTab);
      disassemblyTabs.addTab("&#x2b;").classList.add("open-tab");
      disassemblyView = new DisassemblyView(disassemblyContainer, selectionBroker);
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
      const uploadFile = this.files && this.files[0];
      if (uploadFile) {
        const filereader = new FileReader();
        filereader.onload = () => {
          const txtRes = filereader.result;
          if (typeof txtRes == 'string')
            loadFile(txtRes);
        };
        filereader.readAsText(uploadFile);
      }
    });
  }

  initializeUploadHandlers();
  resizer.updatePanes();
};
