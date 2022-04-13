import explicit from 'explicit-main';
import implicit from 'implicit-main';
import implicitModule from 'implicit-main-type-module';

function getImplicitCommonjs () {
  return import('implicit-main-type-commonjs');
}

export {explicit, implicit, implicitModule, getImplicitCommonjs};
export default 'success';
