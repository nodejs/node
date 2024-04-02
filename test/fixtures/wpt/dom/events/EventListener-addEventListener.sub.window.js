async_test(function(t) {
  let crossOriginFrame = document.createElement('iframe');
  crossOriginFrame.src = 'https://{{hosts[alt][]}}:{{ports[https][0]}}/common/blank.html';
  document.body.appendChild(crossOriginFrame);
  crossOriginFrame.addEventListener('load', t.step_func_done(function() {
    let crossOriginWindow = crossOriginFrame.contentWindow;
    window.addEventListener('click', crossOriginWindow);
  }));
}, "EventListener.addEventListener doesn't throw when a cross origin object is passed in.");
