// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import * as C from "./common/constants";
import { SourceResolver } from "./source-resolver";
import { SelectionBroker } from "./selection/selection-broker";
import { DisassemblyView } from "./views/disassembly-view";
import { GraphMultiView } from "./graphmultiview";
import { CodeMode, CodeView } from "./views/code-view";
import { Tabs } from "./tabs";
import { Resizer } from "./resizer";
import { InfoView } from "./views/info-view";

window.onload = function () {
  let multiview: GraphMultiView;
  let disassemblyView: DisassemblyView;
  let sourceViews: Array<CodeView> = new Array<CodeView>();
  let selectionBroker: SelectionBroker;
  let sourceResolver: SourceResolver;

  const resizer = new Resizer(() => multiview?.onresize(), 75, 75);

  const disassemblyTabsContainer = document.getElementById(C.GENERATED_PANE_ID);
  const disassemblyTabs = new Tabs(disassemblyTabsContainer);
  disassemblyTabs.addTab("&#x2b;").classList.add("last-tab", "persistent-tab");

  const sourceTabsContainer = document.getElementById(C.SOURCE_PANE_ID);
  const sourceTabs = new Tabs(sourceTabsContainer);
  sourceTabs.addTab("&#x2b;").classList.add("last-tab", "persistent-tab");
  const [infoTab, infoContainer] = sourceTabs.addTabAndContent("Info");
  infoTab.classList.add("persistent-tab");
  infoContainer.classList.add("viewpane", "scrollable");
  const infoView = new InfoView(infoContainer);
  infoView.show();
  sourceTabs.activateTab(infoTab);

  function loadFile(txtRes: string): void {
    sourceTabs.clearTabsAndContent();
    disassemblyTabs.clearTabsAndContent();
    // If the JSON isn't properly terminated, assume compiler crashed and
    // add best-guess empty termination
    if (txtRes[txtRes.length - 2] === ",") {
      txtRes += '{"name":"disassembly","type":"disassembly","data":""}]}';
    }
    try {
      sourceViews.forEach(sv => sv.hide());
      multiview?.hide();
      multiview = null;
      document.getElementById("ranges").innerHTML = "";
      document.getElementById("ranges").style.visibility = "hidden";
      document.getElementById("show-hide-ranges").style.visibility = "hidden";
      disassemblyView?.hide();
      sourceViews = new Array<CodeView>();
      sourceResolver = new SourceResolver();
      selectionBroker = new SelectionBroker(sourceResolver);

      const jsonObj = JSON.parse(txtRes);
      const mainFunction = sourceResolver.getMainFunction(jsonObj);

      sourceResolver.setInlinings(jsonObj.inlinings);
      sourceResolver.setSourceLineToBytecodePosition(jsonObj.sourceLineToBytecodePosition);
      sourceResolver.setSources(jsonObj.sources, mainFunction);
      sourceResolver.setNodePositionMap(jsonObj.nodePositions);
      sourceResolver.parsePhases(jsonObj.phases);

      const [sourceTab, sourceContainer] = sourceTabs.addTabAndContent("Source");
      sourceContainer.classList.add("viewpane", "scrollable");
      sourceTabs.activateTab(sourceTab);

      const sourceView = new CodeView(sourceContainer, selectionBroker, sourceResolver,
        mainFunction, CodeMode.MAIN_SOURCE);
      sourceView.show();
      sourceViews.push(sourceView);

      for (const source of sourceResolver.sources) {
        const sourceView = new CodeView(sourceContainer, selectionBroker, sourceResolver,
          source, CodeMode.INLINED_SOURCE);
        sourceView.show();
        sourceViews.push(sourceView);
      }

      const [disassemblyTab, disassemblyContainer] = disassemblyTabs.addTabAndContent("Disassembly");
      disassemblyContainer.classList.add("viewpane", "scrollable");
      disassemblyTabs.activateTab(disassemblyTab);
      disassemblyView = new DisassemblyView(disassemblyContainer, selectionBroker);
      disassemblyView.initializeCode(mainFunction.sourceText);
      if (sourceResolver.disassemblyPhase) {
        disassemblyView.initializePerfProfile(jsonObj.eventCounts);
        disassemblyView.showContent(sourceResolver.disassemblyPhase.data);
        disassemblyView.show();
      }

      multiview = new GraphMultiView(C.INTERMEDIATE_PANE_ID, selectionBroker, sourceResolver);
      multiview.show();
    } catch (err) {
      if (window.confirm("Error: Exception during load of TurboFan JSON file:\n" +
        `error: ${err.message} \nDo you want to clear session storage?`)) {
        window.sessionStorage.clear();
      }
      return;
    }
  }

  function initializeUploadHandlers() {
    // The <input> form #upload-helper with type file can't be a picture.
    // We hence keep it hidden, and forward the click from the picture
    // button #upload.
    document.getElementById("upload").addEventListener("click", e => {
      document.getElementById("upload-helper").click();
      e.stopPropagation();
    });

    document.getElementById("upload-helper").addEventListener("change",
      function (this: HTMLInputElement) {
        const uploadFile = this.files && this.files[0];
        if (uploadFile) {
          const fileReader = new FileReader();
          fileReader.onload = () => {
            const txtRes = fileReader.result;
            if (typeof txtRes === "string") {
              loadFile(txtRes);
            }
          };
          fileReader.readAsText(uploadFile);
        }
      }
    );

    window.addEventListener("keydown", (e: KeyboardEvent) => {
      if (e.keyCode == 76 && e.ctrlKey) { // CTRL + L
        document.getElementById("upload-helper").click();
        e.stopPropagation();
        e.preventDefault();
      }
    });
  }

  initializeUploadHandlers();
  resizer.updatePanes();
};
