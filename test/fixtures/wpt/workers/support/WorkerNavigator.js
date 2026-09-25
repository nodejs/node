(async () => {
  const obj = new Object();
  obj.appName    = navigator.appName;
  obj.appVersion = navigator.appVersion;
  obj.platform   = navigator.platform;
  obj.userAgent  = navigator.userAgent;
  obj.onLine     = navigator.onLine;
  if (navigator.userAgentData) {
    obj.brands = navigator.userAgentData.brands;
    obj.mobile = navigator.userAgentData.mobile;
    obj.platform = navigator.userAgentData.platform;
    const highEntropyValues = await navigator.userAgentData.getHighEntropyValues([
      "architecture", "bitness", "fullVersionList", "model",
      "platformVersion", "uaFullVersion", "wow64", "formFactors",
    ]);
    obj.architecture = highEntropyValues.architecture;
    obj.bitness = highEntropyValues.bitness;
    obj.fullVersionList = highEntropyValues.fullVersionList;
    obj.model = highEntropyValues.model;
    obj.platformVersion = highEntropyValues.platformVersion;
    obj.uaFullVersion = highEntropyValues.uaFullVersion;
    obj.wow64 = highEntropyValues.wow64;
    obj.formFactors = highEntropyValues.formFactors;
    obj.NavigatorUADataExposed = (typeof self.NavigatorUAData != "undefined");
  }
  postMessage(obj);
})();
