selflink@1[.]2[.]3 test/fixtures/selflink
├── @scope/y@1[.]2[.]3 test/fixtures/selflink/node_modules/@scope/y
├─┬ @scope/z@1[.]2[.]3 test/fixtures/selflink/node_modules/@scope/z
│ └── glob@4[.]0[.]5 test/fixtures/selflink/node_modules/foo/node_modules/glob [(]symlink[)]
└─┬ foo@1[.]2[.]3 test/fixtures/selflink/node_modules/foo
  ├─┬ glob@4[.]0[.]5 test/fixtures/selflink/node_modules/foo/node_modules/glob
  │ ├── graceful-fs@3[.]0[.]2 test/fixtures/selflink/node_modules/(foo|@scope/z)/node_modules/glob/node_modules/graceful-fs
  │ ├── inherits@2[.]0[.]1 test/fixtures/selflink/node_modules/(foo|@scope/z)/node_modules/glob/node_modules/inherits
  │ ├─┬ minimatch@1[.]0[.]0 test/fixtures/selflink/node_modules/(foo|@scope/z)/node_modules/glob/node_modules/minimatch
  │ │ ├── lru-cache@2[.]5[.]0 test/fixtures/selflink/node_modules/(foo|@scope/z)/node_modules/glob/node_modules/minimatch/node_modules/lru-cache
  │ │ └── sigmund@1[.]0[.]0 test/fixtures/selflink/node_modules/(foo|@scope/z)/node_modules/glob/node_modules/minimatch/node_modules/sigmund
  │ └── once@1[.]3[.]0 test/fixtures/selflink/node_modules/(foo|@scope/z)/node_modules/glob/node_modules/once
  └── selflink@1[.]2[.]3 test/fixtures/selflink [(]symlink[)]
