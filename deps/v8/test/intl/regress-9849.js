// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let d = new Date(271733878);
d.toLocaleString('en-u-nu-arab');
d.toLocaleString('en-u-nu-arab', {dateStyle : 'full', timeStyle : 'full'});
d.toLocaleString('en-u-nu-roman');
d.toLocaleString('en-u-nu-roman', {dateStyle : 'full', timeStyle : 'full'});
d.toLocaleString('sr-u-nu-roman');
d.toLocaleString('sr-u-nu-roman', {dateStyle : 'full', timeStyle : 'full'});
d.toLocaleString('sr-Cyrl-u-nu-roman');
d.toLocaleString('sr-Cyrl-u-nu-roman', {dateStyle : 'full', timeStyle : 'full'});
d.toLocaleString('zh-u-nu-roman', {dateStyle : 'full', timeStyle : 'full'});
d.toLocaleString('ja-u-nu-cyrl', {dateStyle : 'full', timeStyle : 'full'});
