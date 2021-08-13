# Installation
> `npm install --save @types/debug`

# Summary
This package contains type definitions for debug (https://github.com/visionmedia/debug).

# Details
Files were exported from https://github.com/DefinitelyTyped/DefinitelyTyped/tree/master/types/debug.
## [index.d.ts](https://github.com/DefinitelyTyped/DefinitelyTyped/tree/master/types/debug/index.d.ts)
````ts
// Type definitions for debug 4.1
// Project: https://github.com/visionmedia/debug
// Definitions by: Seon-Wook Park <https://github.com/swook>
//                 Gal Talmor <https://github.com/galtalmor>
//                 John McLaughlin <https://github.com/zamb3zi>
//                 Brasten Sager <https://github.com/brasten>
//                 Nicolas Penin <https://github.com/npenin>
//                 Kristian Brünn <https://github.com/kristianmitk>
//                 Caleb Gregory <https://github.com/calebgregory>
// Definitions: https://github.com/DefinitelyTyped/DefinitelyTyped

declare var debug: debug.Debug & { debug: debug.Debug; default: debug.Debug };

export = debug;
export as namespace debug;

declare namespace debug {
    interface Debug {
        (namespace: string): Debugger;
        coerce: (val: any) => any;
        disable: () => string;
        enable: (namespaces: string) => void;
        enabled: (namespaces: string) => boolean;
        formatArgs: (this: Debugger, args: any[]) => void;
        log: (...args: any[]) => any;
        selectColor: (namespace: string) => string | number;
        humanize: typeof import('ms');

        names: RegExp[];
        skips: RegExp[];

        formatters: Formatters;
    }

    type IDebug = Debug;

    interface Formatters {
        [formatter: string]: (v: any) => string;
    }

    type IDebugger = Debugger;

    interface Debugger {
        (formatter: any, ...args: any[]): void;

        color: string;
        diff: number;
        enabled: boolean;
        log: (...args: any[]) => any;
        namespace: string;
        destroy: () => boolean;
        extend: (namespace: string, delimiter?: string) => Debugger;
    }
}

````

### Additional Details
 * Last updated: Sat, 24 Jul 2021 08:01:14 GMT
 * Dependencies: [@types/ms](https://npmjs.com/package/@types/ms)
 * Global values: `debug`

# Credits
These definitions were written by [Seon-Wook Park](https://github.com/swook), [Gal Talmor](https://github.com/galtalmor), [John McLaughlin](https://github.com/zamb3zi), [Brasten Sager](https://github.com/brasten), [Nicolas Penin](https://github.com/npenin), [Kristian Brünn](https://github.com/kristianmitk), and [Caleb Gregory](https://github.com/calebgregory).
