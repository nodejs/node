import explicit from 'explicit-main';
import implicit from 'implicit-main';
import implicitModule from 'implicit-main-type-module';
import noMain from 'no-main-field';

function getImplicitCommonjs () {
  return import('implicit-main-type-commonjs');
}

export {explicit, implicit, implicitModule, getImplicitCommonjs, noMain};
export default 'success';
