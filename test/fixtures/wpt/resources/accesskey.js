/*
 * Function that sends an accesskey using the proper key combination depending on the browser and OS.
 *
 * This needs that the test imports the following scripts:
 *     <script src="/resources/testdriver.js"></script>
 *     <script src="/resources/testdriver-actions.js"></script>
 *     <script src="/resources/testdriver-vendor.js"></script>
*/
function pressAccessKey(accessKey){
  let controlKey = '\uE009'; // left Control key
  let altKey = '\uE00A'; // left Alt key
  let optionKey = altKey;  // left Option key
  let shiftKey = '\uE008'; // left Shift key
  // There are differences in using accesskey across browsers and OS's.
  // See: // https://developer.mozilla.org/en-US/docs/Web/HTML/Global_attributes/accesskey
  let isMacOSX = navigator.userAgent.indexOf("Mac") != -1;
  let osAccessKey = isMacOSX ? [controlKey, optionKey] : [shiftKey, altKey];
  let actions = new test_driver.Actions();
  // Press keys.
  for (let key of osAccessKey) {
    actions = actions.keyDown(key);
  }
  actions = actions
            .keyDown(accessKey)
            .addTick()
            .keyUp(accessKey);
  osAccessKey.reverse();
  for (let key of osAccessKey) {
    actions = actions.keyUp(key);
  }
  return actions.send();
}


