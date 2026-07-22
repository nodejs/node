// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function linkClickerBackgroundScript() {

  // time in ms.
  let minInterval = 1*1000;
  let maxInterval = 20*1000;
  let pattern = /.*/;
  let enabled = false;

  let animateIconIntervalId;

  // ===========================================================================

  chrome.runtime.onMessage.addListener(function(msg, sender, response) {
    let result;
    if (msg.type == 'update') result = updateFromMessage(msg);
    if (msg.type == 'get') result = getValues();
    response(result);
  });

  // ===========================================================================
  function updateFromMessage(msg) {
    console.log(msg);
    minInterval = Number(msg.minInterval)
    maxInterval = Number(msg.maxInterval);
    if (maxInterval < minInterval) {
      let tmpMin = Math.min(minInterval, maxInterval);
      maxInterval = Math.max(minInterval, maxInterval);
      minInterval = tmpMin;
    }
    pattern = new RegExp(msg.pattern);
    enabled = Boolean(msg.enabled);
    updateTabs();
    scheduleIconAnimation();
    return getValues();
  }

  function getValues() {
    return {
      type: 'update',
      minInterval: minInterval,
      maxInterval: maxInterval,
      pattern: pattern.source,
      enabled: enabled
    }
  }

  function updateTabs() {
    chrome.tabs.query({active: true, currentWindow: true}, function(tabs) {
      let message = getValues();
      for (let i = 0; i < tabs.length; ++i) {
        chrome.tabs.sendMessage(tabs[i].id, message);
      }
    });
  }

  let animationIndex = 0;
  function animateIcon() {
    animationIndex = (animationIndex + 1) % 4;
    chrome.browserAction.setBadgeText( { text: ".".repeat(animationIndex) } );
  }

  function scheduleIconAnimation() {
    chrome.browserAction.setBadgeText( { text: "" } );
    clearInterval(animateIconIntervalId);
    if (enabled) {
      animateIconIntervalId = setInterval(animateIcon, 500);
    }
  }

})();
