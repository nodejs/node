// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Debouncer, DOM, V8CustomElement} from './helper.mjs';

DOM.defineCustomElement(
    'view/tool-tip', (templateText) => class Tooltip extends V8CustomElement {
      _targetNode;
      _content;
      _isHidden = true;
      _debouncedSetData =
          new Debouncer((...args) => this._setData(...args), 500)

      constructor() {
        super(templateText);
        this._intersectionObserver = new IntersectionObserver((entries) => {
          if (entries[0].intersectionRatio <= 0) {
            this.hide();
          } else {
            this.show();
            this.requestUpdate(true);
          }
        });
        document.addEventListener('click', (event) => {
          // Only hide the tooltip if we click anywhere outside of it.
          let target = event.target;
          while (target) {
            if (target == this) return;
            target = target.parentNode;
          }
          this.hide()
        });
      }

      _update() {
        if (!this._targetNode || this._isHidden) return;
        if (!this._targetNode.parentNode) return;
        const rect = this._targetNode.getBoundingClientRect();
        rect.x += rect.width / 2;
        let atRight = this._useRight(rect.x);
        let atBottom = this._useBottom(rect.y);
        if (atBottom) rect.y += rect.height;
        this._setPosition(rect, atRight, atBottom);
        this.requestUpdate(true);
      }

      set data({content, positionOrTargetNode, immediate}) {
        if (immediate) {
          this._debouncedSetData.callNow(content, positionOrTargetNode)
        } else {
          this._debouncedSetData.call(content, positionOrTargetNode)
        }
      }

      _setData(content, positionOrTargetNode) {
        if (positionOrTargetNode.nodeType === undefined) {
          this._targetNode = undefined;
          const position = positionOrTargetNode;
          this._setPosition(
              position, this._useRight(position.x),
              this._useBottom(position.y));
        } else {
          this._setTargetNode(positionOrTargetNode);
        }
        this._setContent(content);
      }

      _setTargetNode(targetNode) {
        this._intersectionObserver.disconnect();
        this._targetNode = targetNode;
        if (targetNode === undefined) return;
        if (!(targetNode instanceof SVGElement)) {
          this._intersectionObserver.observe(targetNode);
        }
        this.requestUpdate(true);
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

      _setContent(content) {
        if (!content) return this.hide();
        this.show();
        if (this._content === content) return;
        this._content = content;

        if (typeof content === 'string') {
          this.contentNode.innerHTML = content;
          this.contentNode.className = 'textContent';
        } else if (content?.nodeType && content?.nodeName) {
          this._setContentNode(content);
        } else {
          if (this.contentNode.firstChild?.localName == 'property-link-table') {
            this.contentNode.firstChild.propertyDict = content;
          } else {
            const node = DOM.element('property-link-table');
            node.instanceLinkButtons = true;
            node.propertyDict = content;
            this._setContentNode(node);
          }
        }
      }

      _setContentNode(content) {
        const newContent = DOM.div();
        newContent.appendChild(content);
        this.contentNode.replaceWith(newContent);
        newContent.id = 'content';
      }

      hide() {
        this._content = undefined;
        if (this._isHidden) return;
        this._isHidden = true;
        this.bodyNode.style.display = 'none';
        this.targetNode = undefined;
      }

      show() {
        if (!this._isHidden) return;
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
