import { sum, getData } from './dependency.cjs';

export const theModuleSum = (a, b) => sum(a, b);

export const theModuleGetData = () => getData();
