// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

assertTrue(/(\u039C)/i.test("\xB5"));
assertTrue(/(\u039C)+/i.test("\xB5"));
assertTrue(/(\u039C)/ui.test("\xB5"));
assertTrue(/(\u039C)+/ui.test("\xB5"));

assertTrue(/(\u03BC)/i.test("\xB5"));
assertTrue(/(\u03BC)+/i.test("\xB5"));
assertTrue(/(\u03BC)/ui.test("\xB5"));
assertTrue(/(\u03BC)+/ui.test("\xB5"));

assertTrue(/(\u03BC)/i.test("\u039C"));
assertTrue(/(\u03BC)+/i.test("\u039C"));
assertTrue(/(\u03BC)/ui.test("\u039C"));
assertTrue(/(\u03BC)+/ui.test("\u039C"));

assertTrue(/(\u0178)/i.test("\xFF"));
assertTrue(/(\u0178)+/i.test("\xFF"));
assertTrue(/(\u0178)/ui.test("\xFF"));
assertTrue(/(\u0178)+/ui.test("\xFF"));
