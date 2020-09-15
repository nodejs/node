// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import { V8CustomElement, defineCustomElement } from "../helper.mjs";
import { FocusEvent } from "../events.mjs";
import { SourcePositionLogEvent } from "../log/sourcePosition.mjs";

defineCustomElement(
  "./map-panel/map-details",
  (templateText) =>
    class MapDetails extends V8CustomElement {
      constructor() {
        super(templateText);
        this.#filePositionNode.addEventListener("click", e =>
          this.handleFilePositionClick(e)
        );
        this.selectedMap = undefined;
      }
      get mapDetails() {
        return this.$("#mapDetails");
      }

      get #filePositionNode() {
        return this.$("#filePositionNode");
      }

      setSelectedMap(value) {
        this.selectedMap = value;
      }

      set mapDetails(map) {
        let details = "";
        let clickableDetails = "";
        if (map) {
          clickableDetails += "ID: " + map.id;
          clickableDetails += "\nSource location: " + map.filePosition;
          details += "\n" + map.description;
          this.setSelectedMap(map);
        }
        this.#filePositionNode.innerText = clickableDetails;
        this.#filePositionNode.classList.add("clickable");
        this.mapDetails.innerText = details;
      }

      handleFilePositionClick() {
        let filePosition =
          this.createSourcePositionLogEvent(
            this.selectedMap.type, this.selectedMap.time,
            this.selectedMap.filePosition, this.selectedMap.script);
        this.dispatchEvent(new FocusEvent(filePosition));
      }

      createSourcePositionLogEvent(type, time, filePositionLine, script) {
        // remove token
        if (!(/\s/.test(filePositionLine))) return;
        filePositionLine = filePositionLine.split(' ');
        let [file, line, col] = filePositionLine[1].split(':');
        let filePosition = new SourcePositionLogEvent(type, time,
          file, line, col, script);
        return filePosition
      }
    }
);
