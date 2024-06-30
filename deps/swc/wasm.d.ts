/* tslint:disable */
/* eslint-disable */
/**
* @param {string} input
* @param {any} options
* @returns {Promise<any>}
*/
export function transform(input: string, options: any): Promise<any>;
/**
* @param {string} input
* @param {any} options
* @returns {any}
*/
export function transformSync(input: string, options: any): any;

export function transform(src: string, opts?: Options): Promise<TransformOutput>;
export function transformSync(src: string, opts?: Options): TransformOutput;


