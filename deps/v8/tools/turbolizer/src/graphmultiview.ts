// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {GraphView} from "./graph-view.js"
import {ScheduleView} from "./schedule-view.js"
import {SequenceView} from "./sequence-view.js"
import {SourceResolver} from "./source-resolver.js"
import {SelectionBroker} from "./selection-broker.js"
import {View, PhaseView} from "./view.js"

export class GraphMultiView extends View {
  sourceResolver: SourceResolver;
  selectionBroker: SelectionBroker;
  graph: GraphView;
  schedule: ScheduleView;
  sequence: SequenceView;
  selectMenu: HTMLSelectElement;
  currentPhaseView: View & PhaseView;

  createViewElement() {
    const pane = document.createElement('div');
    pane.setAttribute('id', "multiview");
    return pane;
  }

  constructor(id, selectionBroker, sourceResolver) {
    super(id);
    const view = this;
    view.sourceResolver = sourceResolver;
    view.selectionBroker = selectionBroker;
    const searchInput = document.getElementById("search-input") as HTMLInputElement;
    searchInput.addEventListener("keyup", e => {
      if (!view.currentPhaseView) return;
      view.currentPhaseView.searchInputAction(searchInput, e)
    });
    searchInput.setAttribute("value", window.sessionStorage.getItem("lastSearch") || "");
    this.graph = new GraphView(id, selectionBroker,
      (phaseName) => view.displayPhaseByName(phaseName));
    this.schedule = new ScheduleView(id, selectionBroker);
    this.sequence = new SequenceView(id, selectionBroker);
    this.selectMenu = (<HTMLSelectElement>document.getElementById('display-selector'));
  }

  initializeSelect() {
    const view = this;
    view.selectMenu.innerHTML = '';
    view.sourceResolver.forEachPhase((phase) => {
      const optionElement = document.createElement("option");
      optionElement.text = phase.name;
      view.selectMenu.add(optionElement);
    });
    this.selectMenu.onchange = function (this: HTMLSelectElement) {
      window.sessionStorage.setItem("lastSelectedPhase", this.selectedIndex.toString());
      view.displayPhase(view.sourceResolver.getPhase(this.selectedIndex));
    }
  }

  show(data, rememberedSelection) {
    super.show(data, rememberedSelection);
    this.initializeSelect();
    const lastPhaseIndex = +window.sessionStorage.getItem("lastSelectedPhase");
    const initialPhaseIndex = this.sourceResolver.repairPhaseId(lastPhaseIndex);
    this.selectMenu.selectedIndex = initialPhaseIndex;
    this.displayPhase(this.sourceResolver.getPhase(initialPhaseIndex));
  }

  initializeContent() { }

  displayPhase(phase) {
    if (phase.type == 'graph') {
      this.displayPhaseView(this.graph, phase.data);
    } else if (phase.type == 'schedule') {
      this.displayPhaseView(this.schedule, phase);
    } else if (phase.type == 'sequence') {
      this.displayPhaseView(this.sequence, phase);
    }
  }

  displayPhaseView(view, data) {
    const rememberedSelection = this.hideCurrentPhase();
    view.show(data, rememberedSelection);
    document.getElementById("middle").classList.toggle("scrollable", view.isScrollable());
    this.currentPhaseView = view;
  }

  displayPhaseByName(phaseName) {
    const phaseId = this.sourceResolver.getPhaseIdByName(phaseName);
    this.selectMenu.selectedIndex = phaseId - 1;
    this.displayPhase(this.sourceResolver.getPhase(phaseId));
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

  deleteContent() {
    this.hideCurrentPhase();
  }

  detachSelection() {
    return null;
  }
}
