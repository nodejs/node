// Copyright (C) 2019 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

import {Draft, produce} from 'immer';
import * as m from 'mithril';

import {Actions} from '../common/actions';
import {RecordConfig} from '../common/state';

import {copyToClipboard} from './clipboard';
import {globals} from './globals';
import {assertExists} from '../base/logging';


declare type Setter<T> = (draft: Draft<RecordConfig>, val: T) => void;
declare type Getter<T> = (cfg: RecordConfig) => T;

// +---------------------------------------------------------------------------+
// | Probe: the rectangular box on the right-hand-side with a toggle box.      |
// +---------------------------------------------------------------------------+

export interface ProbeAttrs {
  title: string;
  img: string|null;
  descr: string;
  isEnabled: Getter<boolean>;
  setEnabled: Setter<boolean>;
}

export class Probe implements m.ClassComponent<ProbeAttrs> {
  view({attrs, children}: m.CVnode<ProbeAttrs>) {
    const onToggle = (enabled: boolean) => {
      const traceCfg = produce(globals.state.recordConfig, draft => {
        attrs.setEnabled(draft, enabled);
      });
      globals.dispatch(Actions.setRecordConfig({config: traceCfg}));
    };

    const enabled = attrs.isEnabled(globals.state.recordConfig);

    return m(
        `.probe${enabled ? '.enabled' : ''}`,
        attrs.img && m('img', {
          src: `assets/${attrs.img}`,
          onclick: () => onToggle(!enabled),
        }),
        m('label',
          m(`input[type=checkbox]`,
            {checked: enabled, oninput: m.withAttr('checked', onToggle)}),
          m('span', attrs.title)),
        m('div', m('div', attrs.descr), m('.probe-config', children)));
  }
}

// +---------------------------------------------------------------------------+
// | Slider: draggable horizontal slider with numeric spinner.                 |
// +---------------------------------------------------------------------------+

export interface SliderAttrs {
  title: string;
  icon?: string;
  cssClass?: string;
  isTime?: boolean;
  unit: string;
  values: number[];
  get: Getter<number>;
  set: Setter<number>;
  min?: number;
  description?: string;
  disabled?: boolean;
}

export class Slider implements m.ClassComponent<SliderAttrs> {
  onValueChange(attrs: SliderAttrs, newVal: number) {
    const traceCfg = produce(globals.state.recordConfig, draft => {
      attrs.set(draft, newVal);
    });
    globals.dispatch(Actions.setRecordConfig({config: traceCfg}));
  }


  onTimeValueChange(attrs: SliderAttrs, hms: string) {
    try {
      const date = new Date(`1970-01-01T${hms}.000Z`);
      if (isNaN(date.getTime())) return;
      this.onValueChange(attrs, date.getTime());
    } catch {
    }
  }

  onSliderChange(attrs: SliderAttrs, newIdx: number) {
    this.onValueChange(attrs, attrs.values[newIdx]);
  }

  view({attrs}: m.CVnode<SliderAttrs>) {
    const id = attrs.title.replace(/[^a-z0-9]/gmi, '_').toLowerCase();
    const maxIdx = attrs.values.length - 1;
    const val = attrs.get(globals.state.recordConfig);
    const min = attrs.min;
    const description = attrs.description;
    const disabled = attrs.disabled;

    // Find the index of the closest value in the slider.
    let idx = 0;
    for (; idx < attrs.values.length && attrs.values[idx] < val; idx++) {
    }

    let spinnerCfg = {};
    if (attrs.isTime) {
      spinnerCfg = {
        type: 'text',
        pattern: '(0[0-9]|1[0-9]|2[0-3])(:[0-5][0-9]){2}',  // hh:mm:ss
        value: new Date(val).toISOString().substr(11, 8),
        oninput: m.withAttr('value', v => this.onTimeValueChange(attrs, v))
      };
    } else {
      spinnerCfg = {
        type: 'number',
        value: val,
        oninput: m.withAttr('value', v => this.onValueChange(attrs, v))
      };
    }
    return m(
        '.slider' + (attrs.cssClass || ''),
        m('header', attrs.title),
        description ? m('header.descr', attrs.description) : '',
        attrs.icon !== undefined ? m('i.material-icons', attrs.icon) : [],
        m(`input[id="${id}"][type=range][min=0][max=${maxIdx}][value=${idx}]
        ${disabled ? '[disabled]' : ''}`,
          {oninput: m.withAttr('value', v => this.onSliderChange(attrs, v))}),
        m(`input.spinner[min=${min !== undefined ? min : 1}][for=${id}]`,
          spinnerCfg),
        m('.unit', attrs.unit));
  }
}

// +---------------------------------------------------------------------------+
// | Dropdown: wrapper around <select>. Supports single an multiple selection. |
// +---------------------------------------------------------------------------+

export interface DropdownAttrs {
  title: string;
  cssClass?: string;
  options: Map<string, string>;
  get: Getter<string[]>;
  set: Setter<string[]>;
}

export class Dropdown implements m.ClassComponent<DropdownAttrs> {
  resetScroll(dom: HTMLSelectElement) {
    // Chrome seems to override the scroll offset on creation without this,
    // even though we call it after having marked the options as selected.
    setTimeout(() => {
      // Don't reset the scroll position if the element is still focused.
      if (dom !== document.activeElement) dom.scrollTop = 0;
    }, 0);
  }

  onChange(attrs: DropdownAttrs, e: Event) {
    const dom = e.target as HTMLSelectElement;
    const selKeys: string[] = [];
    for (let i = 0; i < dom.selectedOptions.length; i++) {
      const item = assertExists(dom.selectedOptions.item(i));
      selKeys.push(item.value);
    }
    const traceCfg = produce(globals.state.recordConfig, draft => {
      attrs.set(draft, selKeys);
    });
    globals.dispatch(Actions.setRecordConfig({config: traceCfg}));
  }

  view({attrs}: m.CVnode<DropdownAttrs>) {
    const options: m.Children = [];
    const selItems = attrs.get(globals.state.recordConfig);
    let numSelected = 0;
    for (const [key, label] of attrs.options) {
      const opts = {value: key, selected: false};
      if (selItems.includes(key)) {
        opts.selected = true;
        numSelected++;
      }
      options.push(m('option', opts, label));
    }
    const label = `${attrs.title} ${numSelected ? `(${numSelected})` : ''}`;
    return m(
        `select.dropdown${attrs.cssClass || ''}[multiple=multiple]`,
        {
          onblur: (e: Event) => this.resetScroll(e.target as HTMLSelectElement),
          onmouseleave: (e: Event) =>
              this.resetScroll(e.target as HTMLSelectElement),
          oninput: (e: Event) => this.onChange(attrs, e),
          oncreate: (vnode) => this.resetScroll(vnode.dom as HTMLSelectElement),
        },
        m('optgroup', {label}, options));
  }
}


// +---------------------------------------------------------------------------+
// | Textarea: wrapper around <textarea>.                                      |
// +---------------------------------------------------------------------------+

export interface TextareaAttrs {
  placeholder: string;
  cssClass?: string;
  get: Getter<string>;
  set: Setter<string>;
  title?: string;
}

export class Textarea implements m.ClassComponent<TextareaAttrs> {
  onChange(attrs: TextareaAttrs, dom: HTMLTextAreaElement) {
    const traceCfg = produce(globals.state.recordConfig, draft => {
      attrs.set(draft, dom.value);
    });
    globals.dispatch(Actions.setRecordConfig({config: traceCfg}));
  }

  view({attrs}: m.CVnode<TextareaAttrs>) {
    return m(
        '.textarea-holder',
        m('header', attrs.title),
        m(`textarea.extra-input${attrs.cssClass || ''}`, {
          onchange: (e: Event) =>
              this.onChange(attrs, e.target as HTMLTextAreaElement),
          placeholder: attrs.placeholder,
          value: attrs.get(globals.state.recordConfig)
        }));
  }
}

// +---------------------------------------------------------------------------+
// | CodeSnippet: command-prompt-like box with code snippets to copy/paste.    |
// +---------------------------------------------------------------------------+

export interface CodeSnippetAttrs {
  text: string;
  hardWhitespace?: boolean;
}

export class CodeSnippet implements m.ClassComponent<CodeSnippetAttrs> {
  view({attrs}: m.CVnode<CodeSnippetAttrs>) {
    return m(
        '.code-snippet',
        m('button',
          {
            title: 'Copy to clipboard',
            onclick: () => copyToClipboard(attrs.text),
          },
          m('i.material-icons', 'assignment')),
        m('code', attrs.text),
    );
  }
}
