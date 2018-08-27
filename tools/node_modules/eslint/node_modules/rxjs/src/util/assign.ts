import { root } from './root';

export function assignImpl(target: Object, ...sources: Object[]) {
  const len = sources.length;
  for (let i = 0; i < len; i++) {
    const source = sources[i];
    for (let k in source) {
      if (source.hasOwnProperty(k)) {
        target[k] = source[k];
      }
    }
  }
  return target;
};

export function getAssign(root: any) {
  return root.Object.assign || assignImpl;
}

export const assign = getAssign(root);