// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import * as C from "./common/constants";
import { SourceResolver } from "./source-resolver";
import { SelectionBroker } from "./selection/selection-broker";
import { DisassemblyView } from "./views/disassembly-view";
import { GraphMultiView } from "./graphmultiview";
import { CodeView } from "./views/code-view";
import { Tabs } from "./tabs";
import { Resizer } from "./resizer";
import { InfoView } from "./views/info-view";
import { HistoryView } from "./views/history-view";
import { CodeMode } from "./views/view";
import { BytecodeSourceView } from "./views/bytecode-source-view";

window.onload = function () {
  let multiview: GraphMultiView;
  let disassemblyView: DisassemblyView;
  let sourceViews: Array<CodeView> = new Array<CodeView>();
  let bytecodeSourceViews: Array<BytecodeSourceView> = new Array<BytecodeSourceView>();
  let historyView: HistoryView;
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
      bytecodeSourceViews.forEach(bsv => bsv.hide());
      multiview?.hide();
      multiview = null;
      document.getElementById("ranges").innerHTML = "";
      document.getElementById("ranges").style.visibility = "hidden";
      document.getElementById("show-hide-ranges").style.visibility = "hidden";
      disassemblyView?.hide();
      historyView?.hide();
      sourceViews = new Array<CodeView>();
      bytecodeSourceViews = new Array<BytecodeSourceView>();
      sourceResolver = new SourceResolver();
      selectionBroker = new SelectionBroker(sourceResolver);

      const jsonObj = JSON.parse(txtRes);
      const mainFunction = sourceResolver.getMainFunction(jsonObj);

      sourceResolver.setInlinings(jsonObj.inlinings);
      sourceResolver.setSourceLineToBytecodePosition(jsonObj.sourceLineToBytecodePosition);
      sourceResolver.setSources(jsonObj.sources, mainFunction);
      sourceResolver.setBytecodeSources(jsonObj.bytecodeSources);
      sourceResolver.setFinalNodeOrigins(jsonObj.nodeOrigins);
      sourceResolver.parsePhases(jsonObj.phases);

      const [sourceTab, sourceContainer] = sourceTabs.addTabAndContent("Source");
      sourceContainer.classList.add("viewpane", "scrollable");
      sourceTabs.activateTab(sourceTab);

      const sourceView = new CodeView(sourceContainer, selectionBroker, mainFunction,
        sourceResolver, CodeMode.MainSource);
      sourceView.show();
      sourceViews.push(sourceView);

      for (const source of sourceResolver.sources) {
        const sourceView = new CodeView(sourceContainer, selectionBroker, source,
          sourceResolver, CodeMode.InlinedSource);
        sourceView.show();
        sourceViews.push(sourceView);
      }

      if (sourceResolver.bytecodeSources.size > 0) {
        const [_, bytecodeContainer] = sourceTabs.addTabAndContent("Bytecode");
        bytecodeContainer.classList.add("viewpane", "scrollable");

        const bytecodeSources = Array.from(sourceResolver.bytecodeSources.values()).reverse();
        for (const source of bytecodeSources) {
          const codeMode = source.sourceId == -1 ? CodeMode.MainSource : CodeMode.InlinedSource;
          const bytecodeSourceView = new BytecodeSourceView(bytecodeContainer, selectionBroker,
            source, sourceResolver, codeMode);
          bytecodeSourceView.show();
          bytecodeSourceViews.push(bytecodeSourceView);
        }
      }

      const [disassemblyTab, disassemblyContainer] = disassemblyTabs.addTabAndContent("Disassembly");
      disassemblyContainer.classList.add("viewpane", "scrollable");
      disassemblyTabs.activateTab(disassemblyTab);
      disassemblyView = new DisassemblyView(disassemblyContainer, selectionBroker);
      disassemblyView.initializeCode(mainFunction.sourceText);
      if (sourceResolver.disassemblyPhase) {
        disassemblyView.initializePerfProfile(jsonObj.eventCounts);
        disassemblyView.showContent(sourceResolver.disassemblyPhase);
        disassemblyView.show();
      }

      multiview = new GraphMultiView(C.INTERMEDIATE_PANE_ID, selectionBroker, sourceResolver);
      multiview.show();

      historyView = new HistoryView(C.HISTORY_ID, selectionBroker, sourceResolver,
        multiview.displayPhaseByName.bind(multiview));
      historyView.show();
    } catch (err) {
      if (window.confirm("Error: Exception during load of TurboFan JSON file:\n" +
        `error: ${err} \nDo you want to clear session storage?`)) {
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
            document.title = uploadFile.name.replace(".json", "");
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
