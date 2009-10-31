for (var key in process.dns) {
  if (process.dns.hasOwnProperty(key)) exports[key] = process.dns[key];
}
