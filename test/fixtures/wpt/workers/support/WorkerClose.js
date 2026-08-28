onmessage = function(evt)
{
    postMessage(evt.data);
    self.close();
}