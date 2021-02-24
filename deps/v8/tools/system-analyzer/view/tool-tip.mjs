// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {DOM, V8CustomElement} from './helper.mjs';

DOM.defineCustomElement(
    'view/tool-tip', (templateText) => class Tooltip extends V8CustomElement {
      _targetNode;
      _content;
      _isHidden = true;
      constructor() {
        super(templateText);
        this._intersectionObserver = new IntersectionObserver((entries) => {
          if (entries[0].intersectionRatio <= 0) {
            this.hide();
          } else {
            this.show();
            this.update(true);
          }
        });
      }

      _update() {
        if (!this._targetNode || this._isHidden) return;
        const rect = this._targetNode.getBoundingClientRect();
        rect.x += rect.width / 2;
        let atRight = this._useRight(rect.x);
        let atBottom = this._useBottom(rect.y);
        if (atBottom) {
          rect.y += rect.height;
        }
        this._setPosition(rect, atRight, atBottom);
        this.update(true);
      }

      set positionOrTargetNode(positionOrTargetNode) {
        if (positionOrTargetNode.nodeType === undefined) {
          this.position = positionOrTargetNode;
        } else {
          this.targetNode = positionOrTargetNode;
        }
      }

      set targetNode(targetNode) {
        this._intersectionObserver.disconnect();
        this._targetNode = targetNode;
        if (targetNode) {
          this._intersectionObserver.observe(targetNode);
          this.update(true);
        }
      }

      set position(position) {
        this._targetNode = undefined;
        this._setPosition(
            position, this._useRight(position.x), this._useBottom(position.y));
      }

      _setPosition(viewportPosition, atRight, atBottom) {
        const horizontalMode = atRight ? 'right' : 'left';
        const verticalMode = atBottom ? 'bottom' : 'top';
        this.bodyNode.className = horizontalMode + ' ' + verticalMode;
        const pageX = viewportPosition.x + window.scrollX;
        this.style.left = `${pageX}px`;
        const pageY = viewportPosition.y + window.scrollY;
        this.style.top = `${pageY}px`;
      }

      _useBottom(viewportY) {
        return viewportY <= 400;
      }

      _useRight(viewportX) {
        return viewportX < document.documentElement.clientWidth / 2;
      }

      set content(content) {
        if (!content) return this.hide();
        this.show();
        if (typeof content === 'string') {
          this.contentNode.innerHTML = content;
          this.contentNode.className = 'textContent';
        } else {
          const newContent = DOM.div();
          newContent.appendChild(content);
          this.contentNode.replaceWith(newContent);
          newContent.id = 'content';
        }
      }

      hide() {
        this._isHidden = true;
        this.bodyNode.style.display = 'none';
        this.targetNode = undefined;
      }

      show() {
        this.bodyNode.style.display = 'block';
        this._isHidden = false;
      }

      get bodyNode() {
        return this.$('#body');
      }

      get contentNode() {
        return this.$('#content');
      }
    });
