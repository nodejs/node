self.onmessage = async (e) => {
  const response = await fetch(e.data);
  const text = await response.text();
  self.postMessage(text);
};
