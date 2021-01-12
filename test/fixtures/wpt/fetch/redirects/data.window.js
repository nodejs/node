// See ../api/redirect/redirect-to-dataurl.any.js for fetch() tests

async_test(t => {
  const img = document.createElement("img");
  img.onload = t.unreached_func();
  img.onerror = t.step_func_done();
  img.src = "../api/resources/redirect.py?location=data:image/png%3Bbase64,iVBORw0KGgoAAAANSUhEUgAAAIUAAABqCAIAAAAdqgU8AAAAAXNSR0IArs4c6QAAAARnQU1BAACxjwv8YQUAAAAJcEhZcwAADsMAAA7DAcdvqGQAAAF6SURBVHhe7dNBDQAADIPA%2Bje92eBxSQUQSLedlQzo0TLQonFWPVoGWjT%2BoUfLQIvGP/RoGWjR%2BIceLQMtGv/Qo2WgReMferQMtGj8Q4%2BWgRaNf%2BjRMtCi8Q89WgZaNP6hR8tAi8Y/9GgZaNH4hx4tAy0a/9CjZaBF4x96tAy0aPxDj5aBFo1/6NEy0KLxDz1aBlo0/qFHy0CLxj/0aBlo0fiHHi0DLRr/0KNloEXjH3q0DLRo/EOPloEWjX/o0TLQovEPPVoGWjT%2BoUfLQIvGP/RoGWjR%2BIceLQMtGv/Qo2WgReMferQMtGj8Q4%2BWgRaNf%2BjRMtCi8Q89WgZaNP6hR8tAi8Y/9GgZaNH4hx4tAy0a/9CjZaBF4x96tAy0aPxDj5aBFo1/6NEy0KLxDz1aBlo0/qFHy0CLxj/0aBlo0fiHHi0DLRr/0KNloEXjH3q0DLRo/EOPloEWjX/o0TLQovEPPVoGWjT%2BoUfLQIvGP/RoGWjR%2BIceJQMPIOzeGc0PIDEAAAAASUVORK5CYII";
}, "<img> fetch that redirects to data: URL");

globalThis.globalTest = null;
async_test(t => {
  globalThis.globalTest = t;
  const script = document.createElement("script");
  script.src = "../api/resources/redirect.py?location=data:text/javascript,(globalThis.globalTest.unreached_func())()";
  script.onerror = t.step_func_done();
  document.body.append(script);
}, "<script> fetch that redirects to data: URL");

async_test(t => {
  const client = new XMLHttpRequest();
  client.open("GET", "../api/resources/redirect.py?location=data:,");
  client.send();
  client.onload = t.unreached_func();
  client.onerror = t.step_func_done();
}, "XMLHttpRequest fetch that redirects to data: URL");
