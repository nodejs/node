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

import * as MicroModal from 'micromodal';
import * as m from 'mithril';

// We need any here so we can accept vnodes with arbitrary attrs.
// tslint:disable-next-line:no-any
export type AnyAttrsVnode = m.Vnode<any, {}>;

interface ModalDefinition {
  title: string;
  content: AnyAttrsVnode;
  buttons: Button[];
}

export interface Button {
  text: string;
  primary: boolean;
  id: string;
  action: () => void;
}

export async function showModal(attrs: ModalDefinition): Promise<void> {
  const modal = document.querySelector('#main-modal') as HTMLElement;
  m.render(
      modal,
      m('.modal-overlay[data-micromodal-close]',
        {tabindex: -1},
        m('.modal-container[aria-labelledby=mm-title][aria-model][role=dialog]',
          m('header.modal-header',
            m('h2.modal-title', {id: 'mm-title'}, attrs.title),
            m('button.modal-close[aria-label=Close Modal]' +
              '[data-micromodal-close]')),
          m('main.modal-content', attrs.content),
          m('footer.modal-footer', ...makeButtons(attrs.buttons)))));
  return new Promise(resolve => {
    MicroModal.show(
        'main-modal', {onClose: () => resolve(), awaitCloseAnimation: true});
  });
}

export function hideModel() {
  MicroModal.close();
}

function makeButtons(buttonDefinition: Button[]): Array<m.Vnode<Button>> {
  const buttons: Array<m.Vnode<Button>> = [];
  buttonDefinition.forEach(button => {
    buttons.push(
        m('button[data-micromodal-close].modal-btn',
          {
            class: button.primary ? 'modal-btn-primary' : '',
            id: button.id,
            onclick: button.action
          },
          button.text));
  });
  return buttons;
}
