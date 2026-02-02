// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let array = [
  {
    'Text':
        `Wikipédia est un projet d’encyclopédie collective en ligne, universelle.`
  },
  {
    'Text': `Wikipédia est définie par des principes fondateurs. Son contenu est
 sous <b11><b20>licence <b30>Creative Commons BY-SA</b30></b20></b11>. Il peut
 être <b12>copié et réutilisé sous la même licence</b12>, sous réserve d\'en
 respecter les conditions..`
  }
];

let json = JSON.stringify(array);
let parsed = JSON.parse(json);

assertEquals(array[1].Text, parsed[1].Text);
