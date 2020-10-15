// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import { GraphView } from "../src/graph-view";
import { ScheduleView } from "../src/schedule-view";
import { SequenceView } from "../src/sequence-view";
import { SourceResolver } from "../src/source-resolver";
import { SelectionBroker } from "../src/selection-broker";
import { View, PhaseView } from "../src/view";

const multiviewID = "multiview";

const toolboxHTML = `
<div class="graph-toolbox">
  <select id="phase-select">
    <option disabled selected>(please open a file)</option>
  </select>
  <input id="search-input" type="text" title="search nodes for regex" alt="search node for regex" class="search-input"
    placeholder="find with regexp&hellip;">
  <label><input id="search-only-visible" type="checkbox" name="instruction-address" alt="Apply search to visible nodes only">only visible</label>
</div>`;

export class GraphMultiView extends View {
  sourceResolver: SourceResolver;
  selectionBroker: SelectionBroker;
  graph: GraphView;
  schedule: ScheduleView;
  sequence: SequenceView;
  selectMenu: HTMLSelectElement;
  currentPhaseView: PhaseView;

  createViewElement() {
    const pane = document.createElement("div");
    pane.setAttribute("id", multiviewID);
    pane.setAttribute("tabindex", "1");
    pane.className = "viewpane";
    return pane;
  }

  hide() {
    this.hideCurrentPhase();
    super.hide();
  }

  constructor(id, selectionBroker, sourceResolver) {
    super(id);
    const view = this;
    view.sourceResolver = sourceResolver;
    view.selectionBroker = selectionBroker;
    const toolbox = document.createElement("div");
    toolbox.className = "toolbox-anchor";
    toolbox.innerHTML = toolboxHTML;
    view.divNode.appendChild(toolbox);
    const searchInput = toolbox.querySelector("#search-input") as HTMLInputElement;
    const onlyVisibleCheckbox = toolbox.querySelector("#search-only-visible") as HTMLInputElement;
    searchInput.addEventListener("keyup", e => {
      if (!view.currentPhaseView) return;
      view.currentPhaseView.searchInputAction(searchInput, e, onlyVisibleCheckbox.checked);
    });
    view.divNode.addEventListener("keyup", (e: KeyboardEvent) => {
      if (e.keyCode == 191) { // keyCode == '/'
        searchInput.focus();
      }
    });
    searchInput.setAttribute("value", window.sessionStorage.getItem("lastSearch") || "");
    this.graph = new GraphView(this.divNode, selectionBroker, view.displayPhaseByName.bind(this),
      toolbox.querySelector(".graph-toolbox"));
    this.schedule = new ScheduleView(this.divNode, selectionBroker);
    this.sequence = new SequenceView(this.divNode, selectionBroker);
    this.selectMenu = toolbox.querySelector("#phase-select") as HTMLSelectElement;
  }

  initializeSelect() {
    const view = this;
    view.selectMenu.innerHTML = "";
    view.sourceResolver.forEachPhase(phase => {
      const optionElement = document.createElement("option");
      let maxNodeId = "";
      if (phase.type == "graph" && phase.highestNodeId != 0) {
        maxNodeId = ` ${phase.highestNodeId}`;
      }
      optionElement.text = `${phase.name}${maxNodeId}`;
      view.selectMenu.add(optionElement);
    });
    this.selectMenu.onchange = function (this: HTMLSelectElement) {
      const phaseIndex = this.selectedIndex;
      window.sessionStorage.setItem("lastSelectedPhase", phaseIndex.toString());
      view.displayPhase(view.sourceResolver.getPhase(phaseIndex));
    };
  }

  show() {
    // Insert before is used so that the display is inserted before the
    // resizer for the RangeView.
    this.container.insertBefore(this.divNode, this.container.firstChild);
    this.initializeSelect();
    const lastPhaseIndex = +window.sessionStorage.getItem("lastSelectedPhase");
    const initialPhaseIndex = this.sourceResolver.repairPhaseId(lastPhaseIndex);
    this.selectMenu.selectedIndex = initialPhaseIndex;
    this.displayPhase(this.sourceResolver.getPhase(initialPhaseIndex));
  }

  displayPhase(phase, selection?: Set<any>) {
    if (phase.type == "graph") {
      this.displayPhaseView(this.graph, phase, selection);
    } else if (phase.type == "schedule") {
      this.displayPhaseView(this.schedule, phase, selection);
    } else if (phase.type == "sequence") {
      this.displayPhaseView(this.sequence, phase, selection);
    }
  }

  displayPhaseView(view: PhaseView, data, selection?: Set<any>) {
    const rememberedSelection = selection ? selection : this.hideCurrentPhase();
    view.initializeContent(data, rememberedSelection);
    this.currentPhaseView = view;
  }

  displayPhaseByName(phaseName, selection?: Set<any>) {
    const phaseId = this.sourceResolver.getPhaseIdByName(phaseName);
    this.selectMenu.selectedIndex = phaseId;
    this.displayPhase(this.sourceResolver.getPhase(phaseId), selection);
  }

  hideCurrentPhase() {
    let rememberedSelection = null;
    if (this.currentPhaseView != null) {
      rememberedSelection = this.currentPhaseView.detachSelection();
      this.currentPhaseView.hide();
      this.currentPhaseView = null;
    }
    return rememberedSelection;
  }

  onresize() {
    if (this.currentPhaseView) this.currentPhaseView.onresize();
  }

  detachSelection() {
    return null;
  }
}
