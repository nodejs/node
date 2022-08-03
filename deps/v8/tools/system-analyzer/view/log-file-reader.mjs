// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import {DOM, FileReader} from './helper.mjs';

DOM.defineCustomElement(
    '../js/log-file-reader',
    (templateText) => class LogFileReader extends FileReader {
      constructor() {
        super(templateText);
      }
    });
