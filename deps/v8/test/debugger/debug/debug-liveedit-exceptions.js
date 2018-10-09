// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

Debug = debug.Debug
function BestEditor() {
  throw 'Emacs';
}

var exception = null;
var results = [];
var log = []

function listener(event, exec_state, event_data, data) {
  if (event != Debug.DebugEvent.Exception) return;
  try {
    var source_line = event_data.sourceLineText();
    print(source_line);
    log.push(source_line);
    switch (results.length) {
      case 0:
        Replace(BestEditor, "Emacs", "Eclipse");
        break;
      case 1:
        Replace(BestEditor, "Eclipse", "Vim");
        break;
      case 2:
        break;
      default:
        assertUnreachable();
    }
  } catch (e) {
    exception = e;
  }
};

function Replace(fun, original, patch) {
  if (fun.toString().indexOf(original) < 0) return;
  %LiveEditPatchScript(fun, Debug.scriptSource(fun).replace(original, patch));
}

Debug.setListener(listener);
Debug.setBreakOnException();

for (var i = 0; i < 3; i++) {
  try {
    BestEditor();
  } catch (e) {
    results.push(e);
  }
}
Debug.setListener(null);

assertNull(exception);
assertEquals(["Emacs", "Eclipse", "Vim"], results);
print(JSON.stringify(log, 1));
assertEquals([
  "  throw 'Emacs';",
  "  throw 'Eclipse';",
  "  throw 'Vim';",
], log);
