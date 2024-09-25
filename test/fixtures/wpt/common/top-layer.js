// This function is a version of test_driver.bless which works while there are
// elements in the top layer:
// https://github.com/web-platform-tests/wpt/issues/41218.
// Pass it the element at the top of the top layer stack.
window.blessTopLayer = async (topLayerElement) => {
  const button = document.createElement('button');
  topLayerElement.append(button);
  let wait_click = new Promise(resolve => button.addEventListener("click", resolve, {once: true}));
  await test_driver.click(button);
  await wait_click;
  button.remove();
};

window.isTopLayer = (el) => {
  // A bit of a hack. Just test a few properties of the ::backdrop pseudo
  // element that change when in the top layer.
  const properties = ['right','background'];
  const testEl = document.createElement('div');
  document.body.appendChild(testEl);
  const computedStyle = getComputedStyle(testEl, '::backdrop');
  const nonTopLayerValues = properties.map(p => computedStyle[p]);
  testEl.remove();
  for(let i=0;i<properties.length;++i) {
    if (getComputedStyle(el,'::backdrop')[properties[i]] !== nonTopLayerValues[i]) {
      return true;
    }
  }
  return false;
};
