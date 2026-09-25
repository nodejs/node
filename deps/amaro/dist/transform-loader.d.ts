import type { LoadFnOutput, LoadHookContext } from "node:module";
export declare function load(url: string, context: LoadHookContext, nextLoad: (url: string, context?: LoadHookContext) => LoadFnOutput | Promise<LoadFnOutput>): Promise<LoadFnOutput>;
