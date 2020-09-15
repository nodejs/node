// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import { V8CustomElement, defineCustomElement } from "./helper.mjs";

defineCustomElement(
  "source-panel",
  (templateText) =>
    class SourcePanel extends V8CustomElement {
      #selectedSourcePositions;
      constructor() {
        super(templateText);
      }
      get script() {
        return this.$('#script');
      }
      get scriptNode() {
        return this.$('.scriptNode');
      }
      set script(script) {
        this.renderSourcePanel(script);
      }
      set selectedSourcePositions(sourcePositions) {
        this.#selectedSourcePositions = sourcePositions;
        this.renderSourcePanelSelectedHighlight();
      }
      get selectedSourcePositions() {
        return this.#selectedSourcePositions;
      }
      highlightSourcePosition(line, col, script) {
        //TODO(zcankara) change setting source to support multiple files
        this.script = script;
        let codeNodes = this.scriptNode.children;
        for (let index = 1; index <= codeNodes.length; index++) {
          if (index != line) continue;
          let lineText = codeNodes[index - 1].innerHTML;
          for (let char = 1; char <= lineText.length; char++) {
            if (char != col) continue;
            let target = char - 1;
            codeNodes[line - 1].innerHTML = lineText.slice(0, target) +
              "<span class='highlight'> </span>" +
              lineText.slice(target, lineText.length);
          }
        }
      }
      createScriptNode() {
        let scriptNode = document.createElement("pre");
        scriptNode.classList.add('scriptNode');
        return scriptNode;
      }
      renderSourcePanel(source) {
        let scriptNode = this.createScriptNode();
        let sourceLines = source.split("\n");
        for (let line = 1; line <= sourceLines.length; line++) {
          let codeNode = document.createElement("code");
          codeNode.classList.add("line" + line);
          codeNode.innerHTML = sourceLines[line - 1] + "\n";
          scriptNode.appendChild(codeNode);
        }
        let oldScriptNode = this.script.childNodes[1];
        this.script.replaceChild(scriptNode, oldScriptNode);
      }
      renderSourcePanelSelectedHighlight() {
        for (const sourcePosition of this.selectedSourcePositions) {
          let line = sourcePosition.line;
          let col = sourcePosition.col;
          let script = sourcePosition.script;
          if (!(line && col && script)) continue;
          this.highlightSourcePosition(line, col, script);
        }
      }
    }
);
