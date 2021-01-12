function loadScript(url, { doc = document }={}) {
  return new Promise((resolve, reject) => {
    const script = doc.createElement('script');
    script.onload = () => resolve();
    script.onerror = () => reject(Error("Script load failed"));
    script.src = url;
    doc.body.appendChild(script);
  })
}

/**
 *
 * @param {Document} document
 * @param {string|URL} url
 * @returns {HTMLAudioElement}
 */
function appendAudio(document, url) {
  const audio = document.createElement('audio');
  audio.muted = true;
  audio.src = url;
  audio.preload = true;
  document.body.appendChild(audio);
  return audio;
}
