use swc_ecma_transforms_base::resolver;
use swc_ecma_transforms_testing::{test, Tester};

use super::*;
use crate::jsx;

fn tr(t: &mut Tester) -> impl Pass {
    let unresolved_mark = Mark::new();
    let top_level_mark = Mark::new();

    (
        resolver(unresolved_mark, top_level_mark, false),
        refresh(
            true,
            Some(RefreshOptions {
                emit_full_signatures: true,
                ..Default::default()
            }),
            t.cm.clone(),
            Some(t.comments.clone()),
            top_level_mark,
        ),
    )
}

test!(
    module,
    ::swc_ecma_parser::Syntax::Es(::swc_ecma_parser::EsSyntax {
        jsx: true,
        ..Default::default()
    }),
    tr,
    normal_function,
    r#"
    function Hello() {
        function handleClick() {}
        return <h1 onClick={handleClick}>Hi</h1>;
    }
    function Bar() {
        return <Hello />;
    }
"#
);

test!(
    module,
    ::swc_ecma_parser::Syntax::Es(::swc_ecma_parser::EsSyntax {
        jsx: true,
        ..Default::default()
    }),
    tr,
    export_function,
    r#"
    export function Hello() {
      function handleClick() {}
      return <h1 onClick={handleClick}>Hi</h1>;
    }
    export default function Bar() {
      return <Hello />;
    }
    function Baz() {
      return <h1>OK</h1>;
    }
    const NotAComp = 'hi';
    export { Baz, NotAComp };
    export function sum() {}
    export const Bad = 42;
"#
);

test!(
    module,
    ::swc_ecma_parser::Syntax::Es(::swc_ecma_parser::EsSyntax {
        jsx: true,
        ..Default::default()
    }),
    tr,
    export_named_arrow_function,
    r#"
    export const Hello = () => {
      function handleClick() {}
      return <h1 onClick={handleClick}>Hi</h1>;
    };
    export let Bar = (props) => <Hello />;
    export default () => {
      // This one should be ignored.
      // You should name your components.
      return <Hello />;
    };
"#
);

test!(
    module,
    ::swc_ecma_parser::Syntax::Es(::swc_ecma_parser::EsSyntax {
        jsx: true,
        ..Default::default()
    }),
    tr,
    reassigned_function,
    // TODO: in the future, we may *also* register the wrapped one.
    r#"
    function Hello() {
      return <h1>Hi</h1>;
    }
    Hello = connect(Hello);
"#
);

test!(
    module,
    ::swc_ecma_parser::Syntax::Es(::swc_ecma_parser::EsSyntax {
        jsx: true,
        ..Default::default()
    }),
    tr,
    pascal_case_only,
    r#"
    function hello() {
      return 2 * 2;
    }
"#
);

test!(
    module,
    ::swc_ecma_parser::Syntax::Es(::swc_ecma_parser::EsSyntax {
        jsx: true,
        ..Default::default()
    }),
    tr,
    anonymous_function_expressions_declaration,
    r#"
    let Hello = function() {
      function handleClick() {}
      return <h1 onClick={handleClick}>Hi</h1>;
    };
    const Bar = function Baz() {
      return <Hello />;
    };
    function sum() {}
    let Baz = 10;
    var Qux;
"#
);

test!(
    module,
    ::swc_ecma_parser::Syntax::Es(::swc_ecma_parser::EsSyntax {
        jsx: true,
        ..Default::default()
    }),
    tr,
    named_arrow_function_expressions_declaration,
    r#"
    let Hello = () => {
      const handleClick = () => {};
      return <h1 onClick={handleClick}>Hi</h1>;
    }
    const Bar = () => {
      return <Hello />;
    };
    var Baz = () => <div />;
    var sum = () => {};
"#
);

test!(
    module,
    ::swc_ecma_parser::Syntax::Es(::swc_ecma_parser::EsSyntax {
        jsx: true,
        ..Default::default()
    }),
    tr,
    ignore_hoc,
    // TODO: we might want to handle HOCs at usage site, however.
    // TODO: it would be nice if we could always avoid registering
    r#"
    let connect = () => {
      function Comp() {
        const handleClick = () => {};
        return <h1 onClick={handleClick}>Hi</h1>;
      }
      return Comp;
    };
    function withRouter() {
      return function Child() {
        const handleClick = () => {};
        return <h1 onClick={handleClick}>Hi</h1>;
      }
    };
"#
);

test!(
    module,
    ::swc_ecma_parser::Syntax::Es(::swc_ecma_parser::EsSyntax {
        jsx: true,
        ..Default::default()
    }),
    tr,
    ignore_complex_definition,
    r#"
    let A = foo ? () => {
      return <h1>Hi</h1>;
    } : null
    const B = (function Foo() {
      return <h1>Hi</h1>;
    })();
    let C = () => () => {
      return <h1>Hi</h1>;
    };
    let D = bar && (() => {
      return <h1>Hi</h1>;
    });
"#
);

test!(
    module,
    ::swc_ecma_parser::Syntax::Es(::swc_ecma_parser::EsSyntax {
        jsx: true,
        ..Default::default()
    }),
    tr,
    ignore_unnamed_function_declarations,
    r#"export default function() {}"#
);

test!(
    module,
    ::swc_ecma_parser::Syntax::Es(::swc_ecma_parser::EsSyntax {
        jsx: true,
        ..Default::default()
    }),
    tr,
    register_likely_hoc_1,
    r#"
    const A = forwardRef(function() {
      return <h1>Foo</h1>;
    });
    export const B = memo(React.forwardRef(() => {
      return <h1>Foo</h1>;
    }));
    export default React.memo(forwardRef((props, ref) => {
      return <h1>Foo</h1>;
    }));
"#
);

test!(
    module,
    ::swc_ecma_parser::Syntax::Es(::swc_ecma_parser::EsSyntax {
        jsx: true,
        ..Default::default()
    }),
    tr,
    register_likely_hoc_2,
    r#"
    export default React.memo(forwardRef(function (props, ref) {
      return <h1>Foo</h1>;
    }));
"#
);

test!(
    module,
    ::swc_ecma_parser::Syntax::Es(::swc_ecma_parser::EsSyntax {
        jsx: true,
        ..Default::default()
    }),
    tr,
    register_likely_hoc_3,
    r#"
    export default React.memo(forwardRef(function Named(props, ref) {
      return <h1>Foo</h1>;
    }));
"#
);

test!(
    module,
    ::swc_ecma_parser::Syntax::Es(::swc_ecma_parser::EsSyntax {
        jsx: true,
        ..Default::default()
    }),
    tr,
    register_likely_hoc_4,
    r#"
    function Foo() {
      return <div>123</div>
    }

    export default memo(Foo)
"#
);

test!(
    module,
    ::swc_ecma_parser::Syntax::Es(::swc_ecma_parser::EsSyntax {
        jsx: true,
        ..Default::default()
    }),
    tr,
    ignore_not_hoc,
    r#"
    const throttledAlert = throttle(function() {
      alert('Hi');
    });
    const TooComplex = (function() { return hello })(() => {});
    if (cond) {
      const Foo = thing(() => {});
    }
"#
);

test!(
    module,
    ::swc_ecma_parser::Syntax::Es(::swc_ecma_parser::EsSyntax {
        jsx: true,
        ..Default::default()
    }),
    tr,
    register_identifiers_used_in_jsx,
    r#"
    import A from './A';
    import Store from './Store';
    Store.subscribe();
    const Header = styled.div`color: red`
    const StyledFactory1 = styled('div')`color: hotpink`
    const StyledFactory2 = styled('div')({ color: 'hotpink' })
    const StyledFactory3 = styled(A)({ color: 'hotpink' })
    const FunnyFactory = funny.factory``;
    let Alias1 = A;
    let Alias2 = A.Foo;
    const Dict = {};
    function Foo() {
      return (
        <div><A /><B /><StyledFactory1 /><StyledFactory2 /><StyledFactory3 /><Alias1 /><Alias2 /><Header /><Dict.X /></div>
      );
    }
    const B = hoc(A);
    // This is currently registered as a false positive:
    const NotAComponent = wow(A);
    // We could avoid it but it also doesn't hurt.
"#
);

// When in doubt, register variables that were used in JSX.
// Foo, Header, and B get registered.
// A doesn't get registered because it's not declared locally.
// Alias doesn't get registered because its definition is just an identifier.
test!(
    module,
    ::swc_ecma_parser::Syntax::Es(::swc_ecma_parser::EsSyntax {
        jsx: true,
        ..Default::default()
    }),
    tr,
    register_identifiers_used_in_create_element,
    r#"
    import A from './A';
    import Store from './Store';
    Store.subscribe();
    const Header = styled.div`color: red`
    const StyledFactory1 = styled('div')`color: hotpink`
    const StyledFactory2 = styled('div')({ color: 'hotpink' })
    const StyledFactory3 = styled(A)({ color: 'hotpink' })
    const FunnyFactory = funny.factory``;
    let Alias1 = A;
    let Alias2 = A.Foo;
    const Dict = {};
    function Foo() {
      return [
        React.createElement(A),
        React.createElement(B),
        React.createElement(StyledFactory1),
        React.createElement(StyledFactory2),
        React.createElement(StyledFactory3),
        React.createElement(Alias1),
        React.createElement(Alias2),
        jsx(Header),
        React.createElement(Dict.X),
      ];
    }
    React.createContext(Store);
    const B = hoc(A);
    // This is currently registered as a false positive:
    const NotAComponent = wow(A);
    // We could avoid it but it also doesn't hurt.
"#
);

test!(
    ignore,
    ::swc_ecma_parser::Syntax::Es(::swc_ecma_parser::EsSyntax {
        jsx: true,
        ..Default::default()
    }),
    tr,
    register_identifiers_used_in_jsx_false_positive,
    r#"
  const A = foo() {}
  const B = () => {
    const A = () => null
    return <A />
  }
"#
);

test!(
    module,
    ::swc_ecma_parser::Syntax::Es(::swc_ecma_parser::EsSyntax {
        jsx: true,
        ..Default::default()
    }),
    tr,
    register_capitalized_identifiers_in_hoc,
    r#"
    function Foo() {
      return <h1>Hi</h1>;
    }
    export default hoc(Foo);
    export const A = hoc(Foo);
    const B = hoc(Foo);
"#
);

test!(
    module,
    ::swc_ecma_parser::Syntax::Es(::swc_ecma_parser::EsSyntax {
        jsx: true,
        ..Default::default()
    }),
    tr,
    register_fn_with_hooks,
    r#"
    export default function App() {
      const [foo, setFoo] = useState(0);
      React.useEffect(() => {});
      return <h1>{foo}</h1>;
    }
"#
);

test!(
    module,
    ::swc_ecma_parser::Syntax::Es(::swc_ecma_parser::EsSyntax {
        jsx: true,
        ..Default::default()
    }),
    tr,
    register_fn_with_hooks_2,
    r#"
    export function Foo() {
      const [foo, setFoo] = useState(0);
      React.useEffect(() => {});
      return <h1>{foo}</h1>;
    }
    function Bar() {
      const [foo, setFoo] = useState(0);
      React.useEffect(() => {});
      return <h1>{foo}</h1>;
    }

    function baz() {
      return (useState(), useState())
    }
"#
);

test!(
    module,
    ::swc_ecma_parser::Syntax::Es(::swc_ecma_parser::EsSyntax {
        jsx: true,
        ..Default::default()
    }),
    tr,
    register_fn_expr_with_hooks,
    r#"
    export const A = React.memo(React.forwardRef((props, ref) => {
      const [foo, setFoo] = useState(0);
      React.useEffect(() => {});
      return <h1 ref={ref}>{foo}</h1>;
    }));
    export const B = React.memo(React.forwardRef(function(props, ref) {
      const [foo, setFoo] = useState(0);
      React.useEffect(() => {});
      return <h1 ref={ref}>{foo}</h1>;
    }));
    function hoc() {
      return function Inner() {
        const [foo, setFoo] = useState(0);
        React.useEffect(() => {});
        return <h1 ref={ref}>{foo}</h1>;
      };
    }
    export let C = hoc();
"#
);

test!(
    module,
    ::swc_ecma_parser::Syntax::Es(::swc_ecma_parser::EsSyntax {
        jsx: true,
        ..Default::default()
    }),
    tr,
    register_fn_expr_with_hooks_2,
    r#"
  const A = function () {
    const [foo, setFoo] = useState(0);
  }, B = () => {
    const [foo, setFoo] = useState(0);
  }
"#
);

test!(
    module,
    ::swc_ecma_parser::Syntax::Es(::swc_ecma_parser::EsSyntax {
        jsx: true,
        ..Default::default()
    }),
    tr,
    register_implicit_arrow_returns,
    r#"
    export default () => useContext(X);
    export const Foo = () => useContext(X);
    module.exports = () => useContext(X);
    const Bar = () => useContext(X);
    const Baz = memo(() => useContext(X));
    const Qux = () => (0, useContext(X));
"#
);

test!(
    module,
    ::swc_ecma_parser::Syntax::Es(::swc_ecma_parser::EsSyntax {
        jsx: true,
        ..Default::default()
    }),
    tr,
    hook_signatures_should_include_custom_hook,
    r#"
    function useFancyState() {
      const [foo, setFoo] = React.useState(0);
      useFancyEffect();
      return foo;
    }
    const useFancyEffect = () => {
      React.useEffect(() => {});
    };
    export default function App() {
      const bar = useFancyState();
      return <h1>{bar}</h1>;
    }
"#
);

test!(
    module,
    ::swc_ecma_parser::Syntax::Typescript(::swc_ecma_parser::TsSyntax {
        tsx: true,
        ..Default::default()
    }),
    |t| {
        let unresolved_mark = Mark::new();
        let top_level_mark = Mark::new();

        (
            resolver(unresolved_mark, top_level_mark, false),
            refresh(
                true,
                Some(RefreshOptions {
                    emit_full_signatures: true,
                    ..Default::default()
                }),
                t.cm.clone(),
                Some(t.comments.clone()),
                top_level_mark,
            ),
            jsx(
                t.cm.clone(),
                Some(t.comments.clone()),
                Default::default(),
                top_level_mark,
                unresolved_mark,
            ),
        )
    },
    include_hook_signature_in_commonjs,
    r#"
    import {useFancyState} from './hooks';
    import useFoo from './foo'
    export default function App() {
      const bar = useFancyState();
      const foo = useFoo()
      return <h1>{bar}</h1>;
    }
"#
);

test!(
    module,
    ::swc_ecma_parser::Syntax::Es(::swc_ecma_parser::EsSyntax {
        jsx: true,
        ..Default::default()
    }),
    tr,
    gen_valid_hook_signatures_for_exotic_hooks,
    r#"
    import FancyHook from 'fancy';
    export default function App() {
      function useFancyState() {
        const [foo, setFoo] = React.useState(0);
        useFancyEffect();
        return foo;
      }
      const bar = useFancyState();
      const baz = FancyHook.useThing();
      React.useState();
      useThePlatform();
      return <h1>{bar}{baz}</h1>;
    }
"#
);

test!(
    module,
    ::swc_ecma_parser::Syntax::Typescript(::swc_ecma_parser::TsSyntax {
        tsx: true,
        ..Default::default()
    }),
    tr,
    dont_consider_require_as_hoc,
    r#"
    const A = require('A');
    const B = foo ? require('X') : require('Y');
    const C = requireCond(gk, 'C');
    const D = import('D');
    import E = require('E');
    export default function App() {
      return (
        <div>
          <A />
          <B />
          <C />
          <D />
          <E />
        </div>
      );
    }
"#
);

test!(
    module,
    ::swc_ecma_parser::Syntax::Es(::swc_ecma_parser::EsSyntax {
        jsx: true,
        ..Default::default()
    }),
    tr,
    should_refresh_when_has_comment,
    r#"
    export function Foo() {
      const [foo, setFoo] = useState(0);
      React.useEffect(() => {});
      return <h1>{foo}</h1>;
    }
    function Bar() {
      const [foo, setFoo] = useState(0);
      React.useEffect(() => {
        // @refresh reset
      });
      return <h1>{foo}</h1>;
    }
"#
);

test!(
    module,
    ::swc_ecma_parser::Syntax::Es(::swc_ecma_parser::EsSyntax {
        jsx: true,
        ..Default::default()
    }),
    tr,
    dont_consider_iife_as_hoc,
    r#"
    while (item) {
      (item => {
        useFoo();
      })(item);
    }
"#
);

test!(
    module,
    ::swc_ecma_parser::Syntax::Es(::swc_ecma_parser::EsSyntax {
        jsx: true,
        ..Default::default()
    }),
    |t| {
        let unresolved_mark = Mark::new();
        let top_level_mark = Mark::new();

        (
            resolver(unresolved_mark, top_level_mark, false),
            refresh(
                true,
                Some(RefreshOptions {
                    refresh_reg: "import_meta_refreshReg".into(),
                    refresh_sig: "import_meta_refreshSig".into(),
                    emit_full_signatures: true,
                }),
                t.cm.clone(),
                Some(t.comments.clone()),
                top_level_mark,
            ),
        )
    },
    custom_identifier,
    r#"
    export default function Bar () {
      useContext(X)
      return <Foo />
    }
"#
);

test!(
    module,
    ::swc_ecma_parser::Syntax::Typescript(::swc_ecma_parser::TsSyntax {
        tsx: true,
        ..Default::default()
    }),
    tr,
    issue_1865,
    r#"
    function useHooks() {
      return useMemo(() => 1);
    }

    declare module 'x' {}
"#
);

test!(
    module,
    Default::default(),
    tr,
    next_001,
    "
    import dynamic from 'next/dynamic'

    export const Comp = dynamic(() => import('../Content'), { ssr: false })
    "
);

test!(
    module,
    Default::default(),
    tr,
    issue_2261,
    "
    export function App() {
      console.log(useState())

      return null;
    }
  "
);

test!(
    module,
    ::swc_ecma_parser::Syntax::Es(::swc_ecma_parser::EsSyntax {
        jsx: true,
        ..Default::default()
    }),
    tr,
    nested_hook,
    r#"
const a = (a) => {
    const useE = useEffect;
    return function useFoo() {
      useE(() => console.log(a), []);
      return useState(123);
    };
}
"#
);

test!(
    module,
    ::swc_ecma_parser::Syntax::Es(::swc_ecma_parser::EsSyntax {
        jsx: true,
        ..Default::default()
    }),
    tr,
    issue_6022,
    r#"/* @refresh reset */
    import { useState } from 'react';

    function Counter() {
      const [count, setCount] = useState(0);

      return (
        <button type="button" onClick={() => setCount(c => c + 1)}>{count}</button>
      );
    }
"#
);
