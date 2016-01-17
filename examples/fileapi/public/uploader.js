function Uploader(url, cb) {
  this.ws = new WebSocket(url);
  if (cb) this.ws.onopen = cb;
  this.sendQueue = [];
  this.sending = null;
  this.sendCallback = null;
  this.ondone = null;
  var self = this;
  this.ws.onmessage = function(event) {
    var data = JSON.parse(event.data);
    if (data.event == 'complete') {
      if (data.path != self.sending.path) {
        self.sendQueue = [];
        self.sending = null;
        self.sendCallback = null;
        throw new Error('Got message for wrong file!');
      }
      self.sending = null;
      var callback = self.sendCallback;
      self.sendCallback = null;
      if (callback) callback();
      if (self.sendQueue.length === 0 && self.ondone) self.ondone(null);
      if (self.sendQueue.length > 0) {
        var args = self.sendQueue.pop();
        setTimeout(function() { self.sendFile.apply(self, args); }, 0);
      }
    }
    else if (data.event == 'error') {
      self.sendQueue = [];
      self.sending = null;
      var callback = self.sendCallback;
      self.sendCallback = null;
      var error = new Error('Server reported send error for file ' + data.path);
      if (callback) callback(error);
      if (self.ondone) self.ondone(error);
    }
  }
}

Uploader.prototype.sendFile = function(file, cb) {
  if (this.ws.readyState != WebSocket.OPEN) throw new Error('Not connected');
  if (this.sending) {
    this.sendQueue.push(arguments);
    return;
  }
  var fileData = { name: file.name, path: file.webkitRelativePath };
  this.sending = fileData;
  this.sendCallback = cb;
  this.ws.send(JSON.stringify(fileData));
  this.ws.send(file);
}

Uploader.prototype.close = function() {
  this.ws.close();
}
