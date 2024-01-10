self.close()
var source = new EventSource("../resources/message.py")
postMessage(source.readyState)