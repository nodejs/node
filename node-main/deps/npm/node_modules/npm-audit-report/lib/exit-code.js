// return 1 if any vulns in the set are at or above the specified severity
const severities = new Map(Object.entries([
  'info',
  'low',
  'moderate',
  'high',
  'critical',
  'none',
]).map(s => s.reverse()))

module.exports = (data, level) =>
  Object.entries(data.metadata.vulnerabilities)
    .some(([sev, count]) => count > 0 && severities.has(sev) &&
      severities.get(sev) >= severities.get(level)) ? 1 : 0
