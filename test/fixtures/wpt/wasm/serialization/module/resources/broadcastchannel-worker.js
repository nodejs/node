const channel = new BroadcastChannel("anne was here");
channel.onmessage = ({ data }) => {
  if(data === "hi" || data === "sw-success") {
    return;
  } else if(data instanceof WebAssembly.Module) {
    channel.postMessage("dw-success");
  }
}
channel.postMessage("hi");
