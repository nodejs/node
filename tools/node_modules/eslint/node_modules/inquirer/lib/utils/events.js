'use strict';
var Rx = require('rxjs/Rx');

function normalizeKeypressEvents(value, key) {
  return { value: value, key: key || {} };
}

module.exports = function(rl) {
  var keypress = Rx.Observable.fromEvent(rl.input, 'keypress', normalizeKeypressEvents)
    // Ignore `enter` key. On the readline, we only care about the `line` event.
    .filter(({ key }) => key.name !== 'enter' && key.name !== 'return');

  return {
    line: Rx.Observable.fromEvent(rl, 'line'),
    keypress: keypress,

    normalizedUpKey: keypress
      .filter(
        ({ key }) =>
          key.name === 'up' || key.name === 'k' || (key.name === 'p' && key.ctrl)
      )
      .share(),

    normalizedDownKey: keypress
      .filter(
        ({ key }) =>
          key.name === 'down' || key.name === 'j' || (key.name === 'n' && key.ctrl)
      )
      .share(),

    numberKey: keypress
      .filter(e => e.value && '123456789'.indexOf(e.value) >= 0)
      .map(e => Number(e.value))
      .share(),

    spaceKey: keypress.filter(({ key }) => key && key.name === 'space').share(),
    aKey: keypress.filter(({ key }) => key && key.name === 'a').share(),
    iKey: keypress.filter(({ key }) => key && key.name === 'i').share()
  };
};
