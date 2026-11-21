import type { LoadFnOutput, LoadHookContext } from "node:module";
type NextLoad = (url: string, context?: Partial<LoadHookContext>) => LoadFnOutput;
export declare function loadSync(url: string, context: LoadHookContext, nextLoad: NextLoad): LoadFnOutput | {
    format: string;
    source: string;
};
export declare function load(url: string, context: LoadHookContext, nextLoad: (url: string, context?: Partial<LoadHookContext>) => LoadFnOutput | Promise<LoadFnOutput>): Promise<LoadFnOutput | {
    format: string;
    source: string;
}>;
export {};
