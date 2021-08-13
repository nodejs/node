// Type definitions for supports-color 8.1
// Project: https://github.com/chalk/supports-color
// Definitions by: Melvin Groenhoff <https://github.com/mgroenhoff>
//                 Matt Traynham <https://github.com/mtraynham>
//                 Piotr Błażejewicz <https://github.com/peterblazejewicz>
// Definitions: https://github.com/DefinitelyTyped/DefinitelyTyped

export namespace supportsColor {
    interface Level {
        level: number;
        hasBasic: boolean;
        has256: boolean;
        has16m: boolean;
    }

    interface Options {
        /**
         * By default it is `true`, which instructs `supportsColor()` to sniff `process.argv`
         * for the multitude of `--color` flags (see _Info_ below).
         * If `false`, then `process.argv` is not considered when determining color support.
         * @default true
         */
        sniffFlags?: boolean | undefined;
        isTTY?: boolean | undefined;
    }

    type SupportsColor = false | Level;
}

export function supportsColor(
    stream: {
        isTTY?: boolean | undefined;
    },
    options?: supportsColor.Options,
): supportsColor.SupportsColor;

export const stdout: supportsColor.SupportsColor;
export const stderr: supportsColor.SupportsColor;
