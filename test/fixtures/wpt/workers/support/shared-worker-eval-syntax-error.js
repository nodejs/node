let errorFired = false;
onerror = () => {
  errorFired = true;
  return false;
};
onconnect = e => {
  e.ports[0].postMessage(errorFired ? "onerror-fired" : "no-error");
};
eval('1 + ;');
