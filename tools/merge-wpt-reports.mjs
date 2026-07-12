import * as fs from 'node:fs';
import * as path from 'node:path';
import * as url from 'node:url';

const __dirname = path.dirname(url.fileURLToPath(import.meta.url));

const outDir = path.resolve(__dirname, '../out/wpt');
const files = fs.readdirSync(outDir);
const reports = files.filter((file) => file.endsWith('.json'));

const startTimes = [];
const endTimes = [];
const results = [];
let runInfo;

for (const file of reports) {
  const report = JSON.parse(fs.readFileSync(path.resolve(outDir, file)));
  fs.unlinkSync(path.resolve(outDir, file));
  results.push(...report.results);
  startTimes.push(report.time_start);
  endTimes.push(report.time_end);
  runInfo ||= report.run_info;
}

const mergedReport = {
  time_start: Math.min(...startTimes),
  time_end: Math.max(...endTimes),
  run_info: runInfo,
  results,
};

fs.writeFileSync(path.resolve(outDir, 'wptreport.json'), JSON.stringify(mergedReport));
