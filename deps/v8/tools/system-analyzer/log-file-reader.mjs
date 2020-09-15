// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import { defineCustomElement, V8CustomElement } from './helper.mjs';

defineCustomElement('log-file-reader', (templateText) =>
  class LogFileReader extends V8CustomElement {
    constructor() {
      super(templateText);
      this.addEventListener('click', e => this.handleClick(e));
      this.addEventListener('dragover', e => this.handleDragOver(e));
      this.addEventListener('drop', e => this.handleChange(e));
      this.$('#file').addEventListener('change', e => this.handleChange(e));
      this.$('#fileReader').addEventListener('keydown',
        e => this.handleKeyEvent(e));
    }

    get section() {
      return this.$('#fileReaderSection');
    }

    updateLabel(text) {
      this.$('#label').innerText = text;
    }

    handleKeyEvent(event) {
      if (event.key == "Enter") this.handleClick(event);
    }

    handleClick(event) {
      this.$('#file').click();
    }

    handleChange(event) {
      // Used for drop and file change.
      event.preventDefault();
      this.dispatchEvent(new CustomEvent(
        'fileuploadstart', { bubbles: true, composed: true }));
      var host = event.dataTransfer ? event.dataTransfer : event.target;
      this.readFile(host.files[0]);
    }

    handleDragOver(event) {
      event.preventDefault();
    }

    connectedCallback() {
      this.$('#fileReader').focus();
    }

    readFile(file) {
      if (!file) {
        this.updateLabel('Failed to load file.');
        return;
      }
      this.$('#fileReader').blur();
      this.section.className = 'loading';
      const reader = new FileReader();
      reader.onload = (e) => {
        try {
          let dataModel = Object.create(null);
          dataModel.file = file;
          dataModel.chunk = e.target.result;
          this.updateLabel('Finished loading \'' + file.name + '\'.');
          this.dispatchEvent(new CustomEvent(
            'fileuploadend', {
            bubbles: true, composed: true,
            detail: dataModel
          }));
          this.section.className = 'success';
          this.$('#fileReader').classList.add('done');
        } catch (err) {
          console.error(err);
          this.section.className = 'failure';
        }
      };
      // Delay the loading a bit to allow for CSS animations to happen.
      setTimeout(() => reader.readAsText(file), 0);
    }
  });
