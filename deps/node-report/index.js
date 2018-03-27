// Main module entry point for node-report

const api = require('./api');

// NODEREPORT_EVENTS env var overrides the defaults
const options = process.env.NODEREPORT_EVENTS || 'exception+fatalerror+signal+apicall';
api.setEvents(options);

exports.triggerReport = api.triggerReport;
exports.getReport = api.getReport;
exports.setEvents = api.setEvents;
exports.setCoreDump = api.setCoreDump;
exports.setSignal = api.setSignal;
exports.setFileName = api.setFileName;
exports.setDirectory = api.setDirectory;
exports.setVerbose = api.setVerbose;
