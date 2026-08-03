// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function $(id) {
  return document.querySelector(id);
}

// ===========================================================================
document.addEventListener('DOMContentLoaded', function () {
  installFormChangeHandler()
});


function installFormChangeHandler() {
  initForm();
  let inputs = document.getElementsByTagName("input");
  for (let i = 0; i < inputs.length; i++){
     inputs[i].onchange = onFormChange;
  }
}

function initForm() {
  chrome.runtime.sendMessage({type:'get'}, function(response) {
    updateFromMessage(response);
  });
}
// ===========================================================================

function updateFromMessage(msg) {
  $("#minInterval").value = msg.minInterval;
  $("#maxInterval").value = msg.maxInterval;
  $("#pattern").value = msg.pattern;
  $("#enabled").checked = msg.enabled;
  $("#minIntervalValue").innerText = msg.minInterval+"ms";
  $("#maxIntervalValue").innerText = msg.maxInterval+"ms";
}

function onFormChange() {
  let minInterval = $("#minInterval").value;
  let maxInterval = $("#maxInterval").value;

  let message = {
    type: 'update',
    minInterval: minInterval,
    maxInterval: maxInterval,
    pattern: $("#pattern").value,
    enabled: $("#enabled").checked
  }
  chrome.runtime.sendMessage(message, function(response) {
    updateFromMessage(response);
  });
}
