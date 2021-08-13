// NOTE: These definitions support NodeJS and TypeScript 3.7.

// NOTE: TypeScript version-specific augmentations can be found in the following paths:
//          - ~/base.d.ts         - Shared definitions common to all TypeScript versions
//          - ~/index.d.ts        - Definitions specific to TypeScript 2.1
//          - ~/ts3.7/base.d.ts   - Definitions specific to TypeScript 3.7
//          - ~/ts3.7/index.d.ts  - Definitions specific to TypeScript 3.7 with assert pulled in

// Reference required types from the default lib:
/// <reference lib="es2020" />
/// <reference lib="esnext.asynciterable" />
/// <reference lib="esnext.intl" />
/// <reference lib="esnext.bigint" />

// Base definitions for all NodeJS modules that are not specific to any version of TypeScript:
/// <reference path="ts3.6/base.d.ts" />

// TypeScript 3.7-specific augmentations:
/// <reference path="assert.d.ts" />
