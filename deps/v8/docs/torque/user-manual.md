---
title: 'V8 Torque user manual'
description: 'This document explains the V8 Torque language, as used in the V8 codebase.'
---
V8 Torque is a language that allows developers contributing to the V8 project to express changes in the VM by focusing on the _intent_ of their changes to the VM, rather than preoccupying themselves with unrelated implementation details. The language was designed to be simple enough to make it easy to directly translate the [ECMAScript specification](https://tc39.es/ecma262/) into an implementation in V8, but powerful enough to express the low-level V8 optimization tricks in a robust way, like creating fast-paths based on tests for specific object-shapes.

Torque will be familiar to V8 engineers and JavaScript developers, combining a TypeScript-like syntax that eases both writing and understanding V8 code with syntax and types that reflects concepts that are already common in the [`CodeStubAssembler`](/blog/csa). With a strong type system and structured control flow, Torque ensures correctness by construction. Torque’s expressiveness is sufficient to express almost all of the functionality that is [currently found in V8’s builtins](/docs/builtin-functions). It also is very interoperable with `CodeStubAssembler` builtins and `macro`s written in C++, allowing Torque code to use hand-written CSA functionality and vice-versa.

Torque provides language constructs to represent high-level, semantically-rich tidbits of V8 implementation, and the Torque compiler converts these morsels into efficient assembly code using the `CodeStubAssembler`. Both Torque’s language structure and the Torque compiler’s error checking ensure correctness in ways that were previously laborious and error-prone with direct usage of the `CodeStubAssembler`. Traditionally, writing optimal code with the `CodeStubAssembler` required V8 engineers to carry a lot of specialized knowledge in their heads — much of which was never formally captured in any written documentation — to avoid subtle pitfalls in their implementation. Without that knowledge, the learning curve for writing efficient builtins was steep. Even armed with the necessary knowledge, non-obvious and non-policed gotchas often led to correctness or [security](https://bugs.chromium.org/p/chromium/issues/detail?id=775888) [bugs](https://bugs.chromium.org/p/chromium/issues/detail?id=785804). With Torque, many of these pitfalls can be avoided and recognized automatically by the Torque compiler.

## Getting started

Most source written in Torque is checked into the V8 repository under [the `src/builtins` directory](https://crsrc.org/c/v8/src/builtins), with the file extension `.tq`. Torque definitions of V8's heap-allocated classes are found alongside their C++ definitions, in `.tq` files with the same name as corresponding C++ files in `src/objects`. The actual Torque compiler can be found under [`src/torque`](https://crsrc.org/c/v8/src/torque). For details on how the compiler works, see [Torque Architecture](architecture.md). Tests for Torque functionality are checked in under [`test/torque`](https://crsrc.org/c/v8/test/torque), [`test/cctest/torque`](https://crsrc.org/c/v8/test/cctest/torque), and [`test/unittests/torque`](https://crsrc.org/c/v8/test/unittests/torque).

To give you a taste of the language, let’s write a V8 builtin that prints “Hello World!”. To do this, we’ll add a Torque `macro` in a test case and call it from the `cctest` test framework.

Begin by opening up the `test/torque/test-torque.tq` file and add the following code at the end (but before the last closing `}`):

```torque
@export
macro PrintHelloWorld(): void {
  Print('Hello world!');
}
```

Next, open up `test/cctest/torque/test-torque.cc` and add the following test case that uses the new Torque code to build a code stub:

```cpp
TEST(HelloWorld) {
  Isolate* isolate(CcTest::InitIsolateOnce());
  CodeAssemblerTester asm_tester(isolate, JSParameterCount(0));
  TestTorqueAssembler m(asm_tester.state());
  {
    m.PrintHelloWorld();
    m.Return(m.UndefinedConstant());
  }
  FunctionTester ft(asm_tester.GenerateCode(), 0);
  ft.Call();
}
```

Then [build the `cctest` executable](/docs/test), and finally execute the `cctest` test to print ‘Hello world’:

```bash
$ out/x64.debug/cctest test-torque/HelloWorld
Hello world!
```

## How Torque generates code

The Torque compiler doesn’t create machine code directly, but rather generates C++ code that calls V8’s existing `CodeStubAssembler` interface. The `CodeStubAssembler` uses the [TurboFan compiler’s](https://v8.dev/docs/turbofan) backend to generate efficient code. Torque compilation therefore requires multiple steps:

1. The `gn` build first runs the Torque compiler. It processes all `*.tq` files. Each Torque file `path/to/file.tq` causes the generation of the following files:
    - `path/to/file-tq-csa.cc` and `path/to/file-tq-csa.h` containing generated CSA macros.
    - `path/to/file-tq.inc` to be included in in a corresponding header `path/to/file.h` containing class definitions.
    - `path/to/file-tq-inl.inc` to be included in the corresponding inline header `path/to/file-inl.h`, containing C++ accessors of class definitions.
    - `path/to/file-tq.cc` containing generated heap verifiers, printers, etc.

    The Torque compiler also generates various other known `.h` files, meant to be consumed by the V8 build.
1. The `gn` build then compiles the generated `-csa.cc` files from step 1 into the `mksnapshot` executable.
1. When `mksnapshot` runs, all of V8’s builtins are generated and packaged in to the snapshot file, including those that are defined in Torque and any other builtins that use Torque-defined functionality.
1. The rest of V8 is built. All of Torque-authored builtins are made accessible via the snapshot file which is linked into V8. They can be called like any other builtin. In addition, the `d8` or `chrome` executable also includes the generated compilation units related to class definitions directly.

Graphically, the build process looks like this:

<figure>
  <img src="/_img/docs/torque/build-process.svg" width="800" height="480" alt="" loading="lazy">
</figure>

## Torque tooling { #tooling }

Basic tooling and development environment support is available for Torque.

- There is a [Visual Studio Code plugin](https://github.com/v8/vscode-torque) for Torque, which uses a custom language server to provide features like go-to-definition.
- There is also a formatting tool that should be used after changing `.tq` files: `tools/torque/format-torque.py -i <filename>`

## Troubleshooting builds involving Torque { #troubleshooting }

Why do you need to know this? Understanding how Torque files get converted into machine code is important because different problems (and bugs) can potentially arise in the different stages of translating Torque into the binary bits embedded in the snapshot:

- If you have a syntax or semantic error in Torque code (i.e. a `.tq` file), the Torque compiler fails. The V8 build aborts during this stage, and you will not see other errors that may be uncovered by later parts of the build.
- Once your Torque code is syntactically correct and passes the Torque compiler’s (more or less) rigorous semantic checks, the build of `mksnapshot` can still fail. This most frequently happens with inconsistencies in external definitions provided in `.tq` files. Definitions marked with the `extern` keyword in Torque code signal to the Torque compiler that the definition of required functionality is found in C++. Currently, the coupling between `extern` definitions from `.tq` files and the C++ code to which those `extern` definitions refer is loose, and there is no verification at Torque-compile time of that coupling. When `extern` definitions don’t match (or in the most subtle cases mask) the functionality that they access in the `code-stub-assembler.h` header file or other V8 headers, the C++ build of `mksnapshot` fails.
- Even once `mksnapshot` successfully builds, it can fail during execution. This might happen because Turbofan fails to compile the generated CSA code, for example because a Torque `static_assert` cannot be verified by Turbofan. Also, Torque-provided builtin that are run during snapshot creation might have a bug. For example, `Array.prototype.splice`, a Torque-authored builtin, is called as part of the JavaScript snapshot initialization process to setup the default JavaScript environment. If there is a bug in the implementation, `mksnapshot` crashes during execution. When `mksnapshot` crashes, it’s sometimes useful to call `mksnapshot` passing the `--gdb-jit-full` flag, which generates extra debug information that provides useful context, e.g. names for Torque-generated builtins in `gdb` stack crawls.
- Of course, even if Torque-authored code makes it through `mksnapshot`, it still may be buggy or crash. Adding test cases to `torque-test.tq` and `torque-test.cc` is a good way to ensure that your Torque code does what you actually expect. If your Torque code does end up crashing in `d8` or `chrome`, the `--gdb-jit-full` flag is again very useful.

## `constexpr`: compile-time vs. run-time { #constexpr }

Understanding the Torque build process is also important to understanding a core feature in the Torque language: `constexpr`.

Torque allows evaluation of expressions in Torque code at runtime (i.e. when V8 builtins are executed as part of executing JavaScript). However, it also allows expressions to be executed at compile time (i.e. as part of the Torque build process and before the V8 library and `d8` executable have even been created).

Torque uses the `constexpr` keyword to indicate that an expression must be evaluated at build-time. Its usage is somewhat analogous to [C++’s `constexpr`](https://en.cppreference.com/w/cpp/language/constexpr): in addition to borrowing the `constexpr` keyword and some of its syntax from C++, Torque similarly uses `constexpr` to indicate the distinction between evaluation at compile-time and runtime.

However, there are some subtle differences in Torque’s `constexpr` semantics. In C++, `constexpr` expressions can be evaluated completely by the C++ compiler. In Torque `constexpr` expressions cannot fully be evaluated by the Torque compiler, but instead map to C++ types, variables and expressions that can be (and must be) fully evaluated when running `mksnapshot`. From the Torque-writer’s perspective, `constexpr` expressions do not generate code executed at runtime, so in that sense they are compile-time, even though they are technically evaluated by C++ code external to Torque that `mksnapshot` runs. So, in Torque, `constexpr` essentially means “`mksnapshot`-time”, not “compile time”.

In combination with generics, `constexpr` is a powerful Torque tool that can be used to automate the generation of multiple very efficient specialized builtins that differ from each other in a small number of specific details that can be anticipated by V8 developers in advance.

## Files

Torque code is packaged in individual source files. Each source file consists of a series of declarations, which themselves can optionally wrapped in a namespace declaration to separate the namespaces of declarations. The following description of the grammar is likely out-of-date. The source-of-truth is [the grammar definition in the Torque compiler](https://crsrc.org/c/v8/src/torque/torque-parser.cc?q=TorqueGrammar::TorqueGrammar), which is written using context-free grammar rules.

A Torque file is a sequence of declarations. The possible declarations are listed [in `torque-parser.cc`](https://crsrc.org/c/v8/src/torque/torque-parser.cc?q=TorqueGrammar::declaration).

## Namespaces

Torque namespaces allow declarations to be in independent namespaces. They are similar to C++ namespaces. They allow you to create declarations that are not automatically visible in other namespaces. They can be nested, and declarations inside a nested namespace can access the declarations in the namespace that contains them without qualification. Declarations that are not explicitly in a namespace declaration are put in a shared global default namespace that is visible to all namespaces. Namespaces can be reopened, allowing them to be defined over multiple files.

For example:

```torque
macro IsJSObject(o: Object): bool { … }  // In default namespace

namespace array {
  macro IsJSArray(o: Object): bool { … }  // In array namespace
};

namespace string {
  // …
  macro TestVisibility() {
    IsJsObject(o); // OK, global namespace visible here
    IsJSArray(o);  // ERROR, not visible in this namespace
    array::IsJSArray(o);  // OK, explicit namespace qualification
  }
  // …
};

namespace array {
  // OK, namespace has been re-opened.
  macro EnsureWriteableFastElements(array: JSArray){ … }
};
```

## Declarations

### Types

Torque is strongly typed. Its type system is the basis for many of the security and correctness guarantees it provides.

For many basic types, Torque doesn’t actually inherently know very much about them. Instead, many types are just loosely coupled with `CodeStubAssembler` and C++ types through explicit type mappings and rely on the C++ compiler to enforce the rigor of that mapping. Such types are realized as abstract types.

#### Abstract types

Torque’s abstract types map directly to C++ compile-time and CodeStubAssembler runtime values. Their declarations specify a name and a relationship to C++ types:

```grammar
AbstractTypeDeclaration :
  type IdentifierName ExtendsDeclaration opt GeneratesDeclaration opt ConstexprDeclaration opt

ExtendsDeclaration :
  extends IdentifierName ;

GeneratesDeclaration :
  generates StringLiteral ;

ConstexprDeclaration :
  constexpr StringLiteral ;
```

`IdentifierName` specifies the name of the abstract type, and `ExtendsDeclaration` optionally specifies the type from which the declared type derives. `GeneratesDeclaration` optionally specifies a string literal which corresponds to the C++ `TNode` type used in `CodeStubAssembler` code to contain a runtime value of its type. `ConstexprDeclaration` is a string literal specifying the C++ type corresponding to the `constexpr` version of the Torque type for build-time (`mksnapshot`-time) evaluation.

Here’s an example from `base.tq` for Torque’s 31- and 32-bit signed integer types:

```torque
type int32 generates 'TNode<Int32T>' constexpr 'int32_t';
type int31 extends int32 generates 'TNode<Int32T>' constexpr 'int31_t';
```

#### Union types

Union types express that a value belongs to one of several possible types. We only allow union types for tagged values, because they can be distinguished at runtime using the map pointer. For example, JavaScript numbers are either Smi values or allocated `HeapNumber` objects.

```torque
type Number = Smi | HeapNumber;
```

Union types satisfy the following equalities:

- `A | B = B | A`
- `A | (B | C) = (A | B) | C`
- `A | B = A` if `B` is a subtype of `A`

It is only allowed to form union types from tagged types because untagged types cannot be distinguished at runtime.

When mapping union types to CSA, the most specific common supertype of all the types of the union type is selected, with the exception of `Number` and `Numeric`, which are mapped to the corresponding CSA union types.

#### Class types

Class types make it possible to define, allocate and manipulate structured objects on the V8 GC heap from Torque code. Each Torque class type must correspond to a subclass of HeapObject in C++ code. In order to minimize the expense of maintaining boilerplate object-accessing code between V8’s C++ and Torque implementation, the Torque class definitions are used to generate the required C++ object-accessing code whenever possible (and appropriate) to reduce the hassle of keeping C++ and Torque synchronized by hand.

```grammar
ClassDeclaration :
  ClassAnnotation* extern opt transient opt class IdentifierName ExtendsDeclaration opt GeneratesDeclaration opt {
    ClassMethodDeclaration*
    ClassFieldDeclaration*
  }

ClassAnnotation :
  @doNotGenerateCppClass
  @generatePrint
  @abstract
  @export
  @noVerifier
  @hasSameInstanceTypeAsParent
  @highestInstanceTypeWithinParentClassRange
  @lowestInstanceTypeWithinParentClassRange
  @reserveBitsInInstanceType ( NumericLiteral )
  @apiExposedInstanceTypeValue ( NumericLiteral )

ClassMethodDeclaration :
  transitioning opt IdentifierName ImplicitParameters opt ExplicitParameters ReturnType opt LabelsDeclaration opt StatementBlock

ClassFieldDeclaration :
  ClassFieldAnnotation* weak opt const opt FieldDeclaration;

ClassFieldAnnotation :
  @noVerifier
  @if ( Identifier )
  @ifnot ( Identifier )

FieldDeclaration :
  Identifier ArraySpecifier opt : Type ;

ArraySpecifier :
  [ Expression ]
```

An example class:

```torque
extern class JSProxy extends JSReceiver {
  target: JSReceiver|Null;
  handler: JSReceiver|Null;
}
```

`extern` signifies that this class is defined in C++, rather than defined only in Torque.

The field declarations in classes implicitly generate field getters and setters that can be used from CodeStubAssembler, e.g.:

```cpp
// In TorqueGeneratedExportedMacrosAssembler:
TNode<HeapObject> LoadJSProxyTarget(TNode<JSProxy> p_o);
void StoreJSProxyTarget(TNode<JSProxy> p_o, TNode<HeapObject> p_v);
```

As described above, the fields defined in Torque classes generate C++ code that removes the need for duplicate boilerplate accessor and heap visitor code. The hand-written definition of JSProxy must inherit from a generated class template, like this:

```cpp
// In js-proxy.h:
class JSProxy : public TorqueGeneratedJSProxy<JSProxy, JSReceiver> {

  // Whatever the class needs beyond Torque-generated stuff goes here...

  // At the end, because it messes with public/private:
  TQ_OBJECT_CONSTRUCTORS(JSProxy)
}

// In js-proxy-inl.h:
TQ_OBJECT_CONSTRUCTORS_IMPL(JSProxy)
```

The generated class provides cast functions, field accessor functions, and field offset constants (e.g. `kTargetOffset` and `kHandlerOffset` in this case) representing the byte offset of each field from the beginning of the class.

##### Class type annotations

Some classes can't use the inheritance pattern shown in the example above. In those cases, the class can specify `@doNotGenerateCppClass`, inherit directly from its superclass type, and include a Torque-generated macro for its field offset constants. Such classes must implement their own accessors and cast functions. Using that macro looks like this:

```cpp
class JSProxy : public JSReceiver {
 public:
  DEFINE_FIELD_OFFSET_CONSTANTS(
      JSReceiver::kHeaderSize, TORQUE_GENERATED_JS_PROXY_FIELDS)
  // Rest of class omitted...
}
```

If the `@generatePrint` annotation is added, then the generator will implement a C++ function that prints the field values as defined by the Torque layout. Using the JSProxy example, the signature would be `void TorqueGeneratedJSProxy<JSProxy, JSReceiver>::JSProxyPrint(std::ostream& os)`, which can be inherited by `JSProxy`.

The Torque compiler also generates verification code for all `extern` classes, unless the class opts out with the `@noVerifier` annotation. For example, the JSProxy class definition above will generate a C++ method `void TorqueGeneratedClassVerifiers::JSProxyVerify(JSProxy o, Isolate* isolate)` which verifies that its fields are valid according to the Torque type definition. It will also generate a corresponding function on the generated class, `TorqueGeneratedJSProxy<JSProxy, JSReceiver>::JSProxyVerify`, which calls the static function from `TorqueGeneratedClassVerifiers`. If you want to add extra verification for a class (such as a range of acceptable values on a number, or a requirement that field `foo` is true if field `bar` is non-null, etc.), then add a `DECL_VERIFIER(JSProxy)` to the C++ class (which hides the inherited `JSProxyVerify`) and implement it in `src/objects-debug.cc`. The first step of any such custom verifier should be to call the generated verifier, such as `TorqueGeneratedClassVerifiers::JSProxyVerify(*this, isolate);`. (To run those verifiers before and after every GC, build with `v8_enable_verify_heap = true` and run with `--verify-heap`.)

`@abstract` indicates that the class itself is not instantiated, and does not have its own instance type: the instance types that logically belong to the class are the instance types of the derived classes.

The `@export` annotation causes the Torque compiler to generate a concrete C++ class (such as `JSProxy` in the example above). This is obviously only useful if you don't want to add any C++ functionality beyond that provided by the Torque-generated code. Cannot be used in conjunction with `extern`. For a class that is defined and used only within Torque, it is most appropriate to use neither `extern` nor `@export`.

`@hasSameInstanceTypeAsParent` indicates classes that have the same instance types as their parent class, but rename some fields, or possibly have a different map. In such cases, the parent class is not abstract.

The annotations `@highestInstanceTypeWithinParentClassRange`, `@lowestInstanceTypeWithinParentClassRange`, `@reserveBitsInInstanceType`, and `@apiExposedInstanceTypeValue` all affect generation of instance types. Generally you can ignore these and be okay. Torque is responsible for assigning a unique value in the enum `v8::internal::InstanceType` for every class so that V8 can determine at runtime the type any object in the JS heap. Torque's assignment of instance types should be adequate in the vast majority of cases, but there are a few cases where we want an instance type for a particular class to be stable across builds, or to be at the beginning or end of the range of instance types assigned to its superclass, or to be a range of reserved values that can be defined outside of Torque.

##### Class fields

As well as plain values, as in the example above, class fields may contain indexed data. Here's an example:

```torque
extern class CoverageInfo extends HeapObject {
  const slot_count: int32;
  slots[slot_count]: CoverageInfoSlot;
}
```

This means that instances of `CoverageInfo` are of varying sizes based on the data in `slot_count`.

Unlike C++, Torque will not implicitly add padding between fields; instead, it will fail and emit an error if fields are not properly aligned. Torque also requires that strong fields, weak fields, and scalar fields be together with other fields of the same category in the field order.

`const` means that a field cannot be altered at runtime (or at least not easily; Torque will fail compilation if you attempt to set it). This is a good idea for length fields, which should only be reset with great care because they would require freeing any released space and might cause data races with a marking thread.
In fact, Torque requires length fields used for indexed data to be `const`.

`weak` at the beginning of a field declaration means that the field is a custom weak reference, as opposed to the `MaybeObject` tagging mechanism for weak fields.
In addition `weak` affects generation of constants such as `kEndOfStrongFieldsOffset` and `kStartOfWeakFieldsOffset`, which is a legacy feature used in some custom  `BodyDescriptor`s and currently also still requires grouping fields marked as `weak` together. We hope to remove this keyword once Torque is fully capable of generating all `BodyDescriptor`s.

If the object stored in a field may be a `MaybeObject`-style weak reference (with the second bit set), then `Weak<T>` should be used in the type and the `weak` keyword should **not** be used. There are still some exceptions to this rule, like this field from `Map`, which can contain some strong and some weak types, and is also marked as `weak` for inclusion in the weak section:

```torque
  weak transitions_or_prototype_info: Map|Weak<Map>|TransitionArray|
      PrototypeInfo|Smi;
```

`@if` and `@ifnot` mark fields that should be included in some build configurations but not others. They accept values from the list in `BuildFlags`, in `src/torque/torque-parser.cc`.

##### Classes defined entirely outside Torque

Some classes are not defined in Torque, but Torque must know about every class because it is responsible for assigning instance types. For this case, classes can be declared with no body, and Torque will generate nothing for them except the instance type. Example:

```torque
extern class OrderedHashMap extends HashTable;
```

#### Shapes

Defining a `shape` looks just like defining a `class` except that it uses the keyword `shape` instead of `class`. A `shape` is a subtype of `JSObject` representing a point-in-time arrangement of in-object properties (in spec-ese, these are "data properties" rather than "internal slots"). A `shape` does not have its own instance type. An object with a particular shape may change and lose that shape at any time because the object might go into dictionary mode and move all of its properties out to a separate backing store.

#### Structs

`struct`s are collections of data that can easily be passed around together. (Completely unrelated to the class named `Struct`.) Like classes, they can include macros that operate on the data. Unlike classes, they also support generics. The syntax looks similar to a class:

```torque
@export
struct PromiseResolvingFunctions {
  resolve: JSFunction;
  reject: JSFunction;
}

struct ConstantIterator<T: type> {
  macro Empty(): bool {
    return false;
  }
  macro Next(): T labels _NoMore {
    return this.value;
  }

  value: T;
}
```

##### Struct annotations

Any struct marked as `@export` will be included with a predictable name in the generated file `gen/torque-generated/csa-types.h`. The name is prepended with `TorqueStruct`, so `PromiseResolvingFunctions` becomes `TorqueStructPromiseResolvingFunctions`.

Struct fields can be marked as `const`, which means they shouldn't be written to. The entire struct can still be overwritten.

##### Structs as class fields

A struct may be used as the type of a class field. In that case, it represents packed, ordered data within the class (otherwise, structs have no alignment requirements). This is particularly useful for indexed fields in classes. As an example, `DescriptorArray` contains an array of three-value structs:

```torque
struct DescriptorEntry {
  key: Name|Undefined;
  details: Smi|Undefined;
  value: JSAny|Weak<Map>|AccessorInfo|AccessorPair|ClassPositions;
}

extern class DescriptorArray extends HeapObject {
  const number_of_all_descriptors: uint16;
  number_of_descriptors: uint16;
  raw_number_of_marked_descriptors: uint16;
  filler16_bits: uint16;
  enum_cache: EnumCache;
  descriptors[number_of_all_descriptors]: DescriptorEntry;
}
```

##### References and Slices

`Reference<T>` and `Slice<T>` are special structs representing pointers to data held within heap objects. They both contain an object and an offset; `Slice<T>` also contains a length. Rather than constructing these structs directly, you can use special syntax: `&o.x` will create a `Reference` to the field `x` within the object `o`, or a `Slice` to the data if `x` is an indexed field. For both references and slices, there are const and mutable versions. For references, these types are written as `&T` and `const &T` for mutable and constant references, respectively. The mutability refers to the data they point to and might not hold globally, that is, you can create const references to mutable data. For slices, there is no special syntax for the types and the two versions are written `ConstSlice<T>` and `MutableSlice<T>`. References can be dereferenced with `*` or `->`, consistent with C++.

References and slices to untagged data can also point to off-heap data.

#### Bitfield structs

A `bitfield struct` represents a collection of numeric data that is packed into a single numeric value. Its syntax looks similar to a normal `struct`, with the addition of the number of bits for each field.

```torque
bitfield struct DebuggerHints extends uint31 {
  side_effect_state: int32: 2 bit;
  debug_is_blackboxed: bool: 1 bit;
  computed_debug_is_blackboxed: bool: 1 bit;
  debugging_id: int32: 20 bit;
}
```

If a bitfield struct (or any other numeric data) is stored within a Smi, it can be represented using the type `SmiTagged<T>`.

#### Function pointer types

Function pointers can only point to builtins defined in Torque, since this guarantees the default ABI. They are especially useful to reduce binary code size.

While function pointer types are anonymous (like in C), they can be bound to a type alias (like a `typedef` in C).

```torque
type CompareBuiltinFn = builtin(implicit context: Context)(Object, Object, Object) => Number;
```

#### Special types

There are two special types indicated by the keywords `void` and `never`. `void` is used as the return type for callables that do not return a value, and `never` is used as the return type for callables that never actually return (i.e. only exit through exceptional paths).

#### Transient types

In V8, heap objects can change layout at runtime. To express object layouts that are subject to change or other temporary assumptions in the type system, Torque supports the concept of a “transient type”. When declaring an abstract type, adding the keyword `transient` marks it as a transient type.

```torque
// A HeapObject with a JSArray map, and either fast packed elements, or fast
// holey elements when the global NoElementsProtector is not invalidated.
transient type FastJSArray extends JSArray
    generates 'TNode<JSArray>';
```

For example, in the case of `FastJSArray`, the transient type is invalidated if the array changes to dictionary elements or if the global `NoElementsProtector` is invalidated. To express this in Torque, annotate all callables that could potentially do that as `transitioning`. For example, calling a JavaScript function can execute arbitrary JavaScript, so it is `transitioning`.

```torque
extern transitioning macro Call(implicit context: Context)
                               (Callable, Object): Object;
```

The way this is policed in the type system is that it is illegal to access a value of a transient type across a transitioning operation.

```torque
const fastArray : FastJSArray = Cast<FastJSArray>(array) otherwise Bailout;
Call(f, Undefined);
return fastArray; // Type error: fastArray is invalid here.
```

#### Enums

Enumerations provide a means to define a set of constants and group them under a name similar to
the enum classes in C++. A declaration is introduced by the `enum` keyword and adheres to the following
syntactical structure:

```grammar
EnumDeclaration :
  extern enum IdentifierName ExtendsDeclaration opt ConstexprDeclaration opt { IdentifierName list+ (, ...) opt }
```

A basic example looks like this:

```torque
extern enum LanguageMode extends Smi {
  kStrict,
  kSloppy
}
```

This declaration defines a new type `LanguageMode`, where the `extends` clause specifies the underlying
type, that is the runtime type used to represent a value of the enum. In this example, this is `TNode<Smi>`,
since this is what the type `Smi` `generates`. A `constexpr LanguageMode` converts to `LanguageMode`
in the generated CSA files since no `constexpr` clause is specified on the enum to replace the default name.
If the `extends` clause is omitted, Torque will generate only the `constexpr` version of the type. The `extern` keyword tells Torque that there is a C++ definition of this enum. Currently, only `extern` enums are supported.

Torque generates a distinct type and constant for each of the enum's entries. Those are defined
inside a namespace that matches the enum's name. Necessary specializations of `FromConstexpr<>` are
generated to convert from the entry's `constexpr` types to the enum type. The value generated for an entry in the C++ files is `<enum-constexpr>::<entry-name>` where `<enum-constexpr>` is the `constexpr` name generated for the enum. In the above example, those are `LanguageMode::kStrict` and `LanguageMode::kSloppy`.

Torque's enumerations work very well together with the `typeswitch` construct, because the
values are defined using distinct types:

```torque
typeswitch(language_mode) {
  case (LanguageMode::kStrict): {
    // ...
  }
  case (LanguageMode::kSloppy): {
    // ...
  }
}
```

If the C++ definition of the enum contains more values than those used in `.tq` files, Torque needs to know that. This is done by declaring the enum 'open' by appending a `...` after the last entry. Consider the `ExtractFixedArrayFlag` for example, where only some of the options are available/used from within
Torque:

```torque
enum ExtractFixedArrayFlag constexpr 'CodeStubAssembler::ExtractFixedArrayFlag' {
  kFixedDoubleArrays,
  kAllFixedArrays,
  kFixedArrays,
  ...
}
```

### Callables

Callables are conceptually like functions in JavaScript or C++, but they have some additional semantics that allow them to interact in useful ways with CSA code and with the V8 runtime. Torque provides several different types of callables: `macro`s, `builtin`s, `runtime`s and `intrinsic`s.

```grammar
CallableDeclaration :
  MacroDeclaration
  BuiltinDeclaration
  RuntimeDeclaration
  IntrinsicDeclaration
```

#### `macro` callables

Macros are a callable that correspond to a chunk of generated CSA-producing C++. `macro`s can either be fully defined in Torque, in which case the CSA code is generated by Torque, or marked `extern`, in which case the implementation must be provided as hand-written CSA code in a CodeStubAssembler class. Conceptually, it’s useful to think of `macro`s of chunks of inlinable CSA code that are inlined at callsites.

`macro` declarations in Torque take the following form:

```grammar
MacroDeclaration :
   transitioning opt macro IdentifierName ImplicitParameters opt ExplicitParameters ReturnType opt LabelsDeclaration opt StatementBlock
  extern transitioning opt macro IdentifierName ImplicitParameters opt ExplicitTypes ReturnType opt LabelsDeclaration opt ;
```

Every non-`extern` Torque `macro` uses the `StatementBlock` body of the `macro` to create a CSA-generating function in its namespace’s generated `Assembler` class. This code looks just like other code that you might find in `code-stub-assembler.cc`, albeit a bit less readable because it’s machine-generated. `macro`s that are marked `extern` have no body written in Torque and simply provide the interface to hand-written C++ CSA code so that it’s usable from Torque.

`macro` definitions specify implicit and explicit parameters, an optional return type and optional labels. Parameters and return types will be discussed in more detail below, but for now it suffices to know that they work somewhat like TypeScript parameters, which as discussed in the Function Types section of the TypeScript documentation [here](https://www.typescriptlang.org/docs/handbook/functions.html).

Labels are a mechanism for exceptional exit from a `macro`. They map 1:1 to CSA labels and are added as `CodeStubAssemblerLabels*`-typed parameters to the C++ method generated for the `macro`. Their exact semantics are discussed below, but for the purpose of a `macro` declaration, the comma-separated list of a `macro`’s labels is optionally provided with the `labels` keywords and positioned after the `macro`’s parameter lists and return type.

Here’s an example from `base.tq` of external and Torque-defined `macro`s:

```torque
extern macro BranchIfFastJSArrayForCopy(Object, Context): never
    labels Taken, NotTaken;
macro BranchIfNotFastJSArrayForCopy(implicit context: Context)(o: Object):
    never
    labels Taken, NotTaken {
  BranchIfFastJSArrayForCopy(o, context) otherwise NotTaken, Taken;
}
```

#### `builtin` callables

`builtin`s are similar to `macro`s in that they can either be fully defined in Torque or marked `extern`. In the Torque-based builtin case, the body for the builtin is used to generate a V8 builtin that can be called just like any other V8 builtin, including automatically adding the relevant information in `builtin-definitions.h`. Like `macro`s, Torque `builtin`s that are marked `extern` have no Torque-based body and simply provide an interface to existing V8 `builtin`s so that they can be used from Torque code.

`builtin` declarations in Torque have the following form:

```grammar
MacroDeclaration :
  transitioning opt javascript opt builtin IdentifierName ImplicitParameters opt ExplicitParametersOrVarArgs ReturnType opt StatementBlock
  extern transitioning opt javascript opt builtin IdentifierName ImplicitParameters opt ExplicitTypesOrVarArgs ReturnType opt ;
```

There is only one copy of the code for a Torque builtin, and that is in the generated builtin code object. Unlike `macro`s, when `builtin`s are called from Torque code, the CSA code is not inlined at the callsite, but instead a call is generated to the builtin.

`builtin`s cannot have labels.

If you are coding the implementation of a `builtin`, you can craft a [tailcall](https://en.wikipedia.org/wiki/Tail_call) to a builtin or a runtime function iff (if and only if) it's the final call in the builtin. The compiler may be able to avoid creating a new stack frame in this case. Simply add `tail` before the call, as in `tail MyBuiltin(foo, bar);`.

#### `runtime` callables

`runtime`s are similar to `builtin`s in that they can expose an interface to external functionality to Torque. However, instead of being implemented in CSA, the functionality provided by a `runtime` must always be implemented in the V8 as a standard runtime callback.

`runtime` declarations in Torque have the following form:

```grammar
MacroDeclaration :
  extern transitioning opt runtime IdentifierName ImplicitParameters opt ExplicitTypesOrVarArgs ReturnType opt ;
```

The `extern runtime` specified with name <i>IdentifierName</i> corresponds to the runtime function specified by <code>Runtime::k<i>IdentifierName</i></code>.

Like `builtin`s, `runtime`s cannot have labels.

You can also call a `runtime` function as a tailcall when appropriate. Simply include the `tail` keyword before the call.

Runtime function declarations are often placed in a namespace called `runtime`. This disambiguates them from builtins of the same name and makes it easier to see at the callsite that we are calling a runtime function. We should consider making this mandatory.

#### `intrinsic` callables

`intrinsic`s are builtin Torque callables that provide access to internal functionality that can’t be otherwise implemented in Torque. They are declared in Torque, but not defined, since the implementation is provided by the Torque compiler. `intrinsic` declarations use the following grammar:

```grammar
IntrinsicDeclaration :
  intrinsic % IdentifierName ImplicitParameters opt ExplicitParameters ReturnType opt ;
```

For the most part, “user” Torque code should rarely have to use `intrinsic`s directly.
The following are some of the supported intrinsics:

```torque
// %RawObjectCast downcasts from Object to a subtype of Object without
// rigorous testing if the object is actually the destination type.
// RawObjectCasts should *never* (well, almost never) be used anywhere in
// Torque code except for in Torque-based UnsafeCast operators preceded by an
// appropriate type assert()
intrinsic %RawObjectCast<A: type>(o: Object): A;

// %RawPointerCast downcasts from RawPtr to a subtype of RawPtr without
// rigorous testing if the object is actually the destination type.
intrinsic %RawPointerCast<A: type>(p: RawPtr): A;

// %RawConstexprCast converts one compile-time constant value to another.
// Both the source and destination types should be 'constexpr'.
// %RawConstexprCast translate to static_casts in the generated C++ code.
intrinsic %RawConstexprCast<To: type, From: type>(f: From): To;

// %FromConstexpr converts a constexpr value into into a non-constexpr
// value. Currently, only conversion to the following non-constexpr types
// are supported: Smi, Number, String, uintptr, intptr, and int32
intrinsic %FromConstexpr<To: type, From: type>(b: From): To;

// %Allocate allocates an uninitialized object of size 'size' from V8's
// GC heap and "reinterpret casts" the resulting object pointer to the
// specified Torque class, allowing constructors to subsequently use
// standard field access operators to initialize the object.
// This intrinsic should never be called from Torque code. It's used
// internally when desugaring the 'new' operator.
intrinsic %Allocate<Class: type>(size: intptr): Class;
```

Like `builtin`s and `runtime`s, `intrinsic`s cannot have labels.

### Explicit parameters

Declarations of Torque-defined Callables, e.g. Torque `macro`s and `builtin`s, have explicit parameter lists. They are a list of identifier and type pairs using a syntax reminiscent of typed TypeScript function parameter lists, with the exception that Torque doesn’t support optional parameters or default parameters. Moreover, Torque-implement `builtin`s can optionally support rest parameters if the builtin uses V8’s internal JavaScript calling convention (e.g. is marked with the `javascript` keyword).

```grammar
ExplicitParameters :
  ( ( IdentifierName : TypeIdentifierName ) list* )
  ( ( IdentifierName : TypeIdentifierName ) list+ (, ... IdentifierName ) opt )
```

As an example:

```torque
javascript builtin ArraySlice(
    (implicit context: Context)(receiver: Object, ...arguments): Object {
  // …
}
```

### Implicit parameters

Torque callables can specify implicit parameters using something similar to [Scala’s implicit parameters](https://docs.scala-lang.org/tour/implicit-parameters.html):

```grammar
ImplicitParameters :
  ( implicit ( IdentifierName : TypeIdentifierName ) list* )
```

Concretely: A `macro` can declare implicit parameters in addition to explicit ones:

```torque
macro Foo(implicit context: Context)(x: Smi, y: Smi)
```

When mapping to CSA, implicit parameters and explicit parameters are treated the same and form a joint parameter list.

Implicit parameters are not mentioned at the callsite, but instead are passed implicitly: `Foo(4, 5)`. For this to work, `Foo(4, 5)` must be called in a context that provides a value named `context`. Example:

```torque
macro Bar(implicit context: Context)() {
  Foo(4, 5);
}
```

In contrast to Scala, we forbid this if the names of the implicit parameters are not identical.

Since overload resolution can cause confusing behavior, we ensure that implicit parameters do not influence overload resolution at all. That is: when comparing candidates of an overload set, we do not consider the available implicit bindings at the call-site. Only after we found a single best overload, we check if implicit bindings for the implicit parameters are available.

Having the implicit parameters left of the explicit parameters is different from Scala, but maps better to the existing convention in CSA to have the `context` parameter first.

#### `js-implicit`

For builtins with JavaScript linkage defined in Torque, you should use the keyword `js-implicit` instead of `implicit`. The arguments are limited to these four components of the calling convention:

- context: `NativeContext`
- receiver: `JSAny` (`this` in JavaScript)
- target: `JSFunction` (`arguments.callee` in JavaScript)
- newTarget: `JSAny` (`new.target` in JavaScript)

They don’t all have to be declared, only the ones you want to use. For an example, here is our code for `Array.prototype.shift`:

```torque
  // https://tc39.es/ecma262/#sec-array.prototype.shift
  transitioning javascript builtin ArrayPrototypeShift(
      js-implicit context: NativeContext, receiver: JSAny)(...arguments): JSAny {
  ...
```

Note that the `context` argument is a `NativeContext`. This is because builtins in V8 always embed the native context in their closures. Encoding this in the js-implicit convention allows the programmer to eliminate an operation to load the native context from the function context.

### Overload resolution

Torque `macro`s and operators (which are just aliases for `macro`s) allow for argument-type overloading. The overloading rules are inspired by the ones of C++: an overload is selected if it is strictly better than all alternatives. This means that it has to be strictly better in at least one parameter, and better or equally good in all others.

When comparing a pair of corresponding parameters of two overloads…

- …they are considered equally good if:
    - they are equal;
    - both require some implicit conversion.
- …one is considered better if:
    - it is a strict subtype of the other;
    - it doesn’t require an implicit conversion, while the other does.

If no overload is strictly better than all alternatives, this results in a compile error.

### Deferred blocks

A statement block can optionally be marked as `deferred`, which is a signal to the compiler that it's entered less often. The compiler may choose to locate these blocks at the end of the function, thus improving cache locality for the non-deferred regions of code. For example, in this code from the `Array.prototype.forEach` implementation, we expect to remain on the "fast" path, and only rarely take the bailout case:

```torque
  let k: Number = 0;
  try {
    return FastArrayForEach(o, len, callbackfn, thisArg)
        otherwise Bailout;
  }
  label Bailout(kValue: Smi) deferred {
    k = kValue;
  }
```

Here is another example, where the dictionary elements case is marked as deferred to improve code generation for the more likely cases (from the `Array.prototype.join` implementation):

```torque
  if (IsElementsKindLessThanOrEqual(kind, HOLEY_ELEMENTS)) {
    loadFn = LoadJoinElement<FastSmiOrObjectElements>;
  } else if (IsElementsKindLessThanOrEqual(kind, HOLEY_DOUBLE_ELEMENTS)) {
    loadFn = LoadJoinElement<FastDoubleElements>;
  } else if (kind == DICTIONARY_ELEMENTS)
    deferred {
      const dict: NumberDictionary =
          UnsafeCast<NumberDictionary>(array.elements);
      const nofElements: Smi = GetNumberDictionaryNumberOfElements(dict);
      // <etc>...
```

## Porting CSA code to Torque

[The patch that ported `Array.of`](https://chromium-review.googlesource.com/c/v8/v8/+/1296464) serves as a minimal example of porting CSA code to Torque.
