/* eslint-disable no-console */
'use strict';
const {
  ArrayPrototypeFilter,
  ArrayPrototypeMap,
  ArrayPrototypeReduce,
  NumberPrototypeToFixed,
  ObjectEntries,
  DatePrototypeGetTime,
} = primordials;

const path = require('path');
const { treeToXML } = require('internal/test_runner/utils');

export default async function* coberturaReporter(source) {
  yield '<?xml version="1.0" ?>\n';
  yield '<!DOCTYPE coverage SYSTEM "http://cobertura.sourceforge.net/xml/coverage-04.dtd">\n';

  for await (const event of source) {
    switch (event.type) {
      case 'test:coverage': {
        const { totals, workingDirectory, files } = event.data.summary;
        const rootName = path.parse(workingDirectory).name;

        // separate test files becomes "classes" in terminology of cobertura
        const classes = files.map((f) => {
          const relativePath = path.relative(workingDirectory, f.path);
          const dirname = path.dirname(relativePath);
          const withReplacesSep = dirname.replace(new RegExp(path.sep), '.');

          const branchesByLine = ArrayPrototypeReduce(
            f.branches,
            (acc, b) => {
              if (!acc[b.line]) {
                acc[b.line] = [];
              }

              acc[b.line].push(b);

              return acc;
            },
            {}
          );

          return {
            filename: relativePath,
            packageName:
              dirname === '.' ? rootName : `${rootName}.${withReplacesSep}`,
            className: path.basename(f.path),
            classLineRate: NumberPrototypeToFixed(
              f.coveredLinePercent / 100,
              4
            ),
            classTotalLineCount: f.totalLineCount,
            classCoveredLineCount: f.coveredLineCount,
            classBranchRate: NumberPrototypeToFixed(
              f.coveredBranchPercent / 100,
              4
            ),
            classTotalBranchCount: f.totalBranchCount,
            classCoveredBranchCount: f.coveredBranchCount,
            methods: ArrayPrototypeMap(f.functions, (fn, i) => ({
              name: fn.name || `(anonymous_${i})`,
              hits: fn.count,
              signature: '()V',
              number: fn.line,
            })),
            lines: ArrayPrototypeMap(f.lines, (l) => {
              const branches = branchesByLine[l.line] || [];
              const total = branches.length;
              const covered = ArrayPrototypeFilter(
                branches,
                (b) => b.count !== 0
              ).length;

              const conditionCoverage = branches.length
                ? `${(covered / total) * 100}% (${covered}/${total})`
                : undefined;

              return {
                branch: branches.length !== 0,
                number: l.line,
                hits: l.count,
                conditionCoverage,
              };
            }),
          };
        });

        // packages are folders where children are direct tests within that folder
        const packages = ArrayPrototypeReduce(
          classes,
          (acc, c) => {
            if (!acc[c.packageName]) {
              acc[c.packageName] = {
                children: [],
                packageTotalLineCount: 0,
                packageCoveredLineCount: 0,
                packageTotalBranchCount: 0,
                packageCoveredBranchCount: 0,
              };
            }

            acc[c.packageName].children.push(c);

            acc[c.packageName].packageTotalLineCount += c.classTotalLineCount;
            acc[c.packageName].packageCoveredLineCount +=
              c.classCoveredLineCount;
            acc[c.packageName].packageTotalBranchCount +=
              c.classTotalBranchCount;
            acc[c.packageName].packageCoveredBranchCount +=
              c.classCoveredBranchCount;

            return acc;
          },
          {}
        );

        const packageTree = {
          tag: 'packages',
          attrs: {},
          nesting: 0,
          children: ObjectEntries(packages).map(({ 0: key, 1: value }) => ({
            tag: 'package',
            attrs: {
              name: key,
              'line-rate': NumberPrototypeToFixed(
                value.packageCoveredLineCount / value.packageTotalLineCount,
                4
              ),
              'branch-rate': NumberPrototypeToFixed(
                value.packageCoveredBranchCount / value.packageTotalBranchCount,
                4
              ),
            },
            nesting: 1,
            children: ArrayPrototypeMap(value.children, (c) => ({
              tag: 'class',
              attrs: {
                name: c.className,
                filename: c.filename,
                'line-rate': c.classLineRate,
                'branch-rate': c.classBranchRate,
              },
              nesting: 2,
              children: [
                {
                  tag: 'methods',
                  attrs: {},
                  nesting: 3,
                  children: ArrayPrototypeMap(c.methods, (m) => ({
                    tag: 'method',
                    attrs: {
                      name: m.name,
                      hits: m.hits,
                      signature: m.signature,
                    },
                    nesting: 4,
                    children: [
                      {
                        tag: 'lines',
                        attrs: {},
                        nesting: 5,
                        children: [
                          {
                            tag: 'line',
                            attrs: {
                              number: m.number,
                              hits: m.hits,
                            },
                            nesting: 6,
                          },
                        ],
                      },
                    ],
                  })),
                },
                {
                  tag: 'lines',
                  attrs: {},
                  nesting: 3,
                  children: ArrayPrototypeMap(c.lines, (l) => {
                    const attrs = {
                      number: l.number,
                      hits: l.hits,
                      branch: l.branch,
                    };

                    if (l.branch) {
                      attrs['condition-coverage'] = l.conditionCoverage;
                    }

                    return { tag: 'line', nesting: 4, attrs };
                  }),
                },
              ],
            })),
          })),
        };

        const result = {
          tag: 'coverage',
          attrs: {
            'lines-valid': totals.totalLineCount,
            'lines-covered': totals.coveredLineCount,
            'line-rate': NumberPrototypeToFixed(
              totals.coveredLineCount / totals.totalLineCount,
              4
            ),
            'branches-valid': totals.totalBranchCount,
            'branches-covered': totals.coveredBranchCount,
            'branch-rate': NumberPrototypeToFixed(
              totals.coveredBranchCount / totals.totalBranchCount,
              4
            ),
            timestamp: DatePrototypeGetTime(new Date()),
            complexity: '0',
            version: '0.1',
          },
          nesting: -1,
          children: [
            {
              tag: 'sources',
              attrs: {},
              children: [
                {
                  tag: 'source',
                  attrs: {},
                  children: [workingDirectory],
                  nesting: 1,
                },
              ],
              nesting: 0,
            },
            packageTree,
          ],
        };

        yield treeToXML(result);

        break;
      }
      default:
        break;
    }
  }
}
