// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import { V8CustomElement, defineCustomElement } from "../helper.mjs";
import { FocusEvent } from "../events.mjs";

defineCustomElement(
  "./map-panel/map-details",
  (templateText) =>
    class MapDetails extends V8CustomElement {
      constructor() {
        super(templateText);
        this.mapDetails.addEventListener("click", () =>
          this.handleClickSourcePositions()
        );
        this.selectedMap = undefined;
      }
      get mapDetails() {
        return this.$("#mapDetails");
      }

      setSelectedMap(value) {
        this.selectedMap = value;
      }

      set mapDetails(map) {
        let details = "";
        if (map) {
          details += "ID: " + map.id;
          details += "\nSource location: " + map.filePosition;
          details += "\n" + map.description;
          this.setSelectedMap(map);
        }
        this.mapDetails.innerText = details;
      }

      handleClickSourcePositions() {
        this.dispatchEvent(new FocusEvent(this.selectedMap.filePosition));
      }
    }
);
