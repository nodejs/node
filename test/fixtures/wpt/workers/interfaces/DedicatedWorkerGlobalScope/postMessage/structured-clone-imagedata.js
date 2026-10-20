onmessage = function(e) {
  var imagedata = e.data;
  imagedata.data[0] = 128;
  postMessage(imagedata);
}