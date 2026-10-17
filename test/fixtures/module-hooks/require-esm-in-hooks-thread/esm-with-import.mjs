import { sep } from 'node:path';
import { value } from './dep.mjs';

export const result = `${value}${typeof sep}`;
