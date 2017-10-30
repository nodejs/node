# protoduck [![npm version](https://img.shields.io/npm/v/protoduck.svg)](https://npm.im/protoduck) [![license](https://img.shields.io/npm/l/protoduck.svg)](https://npm.im/protoduck) [![Travis](https://img.shields.io/travis/zkat/protoduck.svg)](https://travis-ci.org/zkat/protoduck) [![AppVeyor](https://ci.appveyor.com/api/projects/status/github/zkat/protoduck?svg=true)](https://ci.appveyor.com/project/zkat/protoduck) [![Coverage Status](https://coveralls.io/repos/github/zkat/protoduck/badge.svg?branch=latest)](https://coveralls.io/github/zkat/protoduck?branch=latest)

[`protoduck`](https://github.com/zkat/protoduck) is a JavaScript library is a
library for making groups of methods, called "protocols".

If you're familiar with the concept of ["duck
typing"](https://en.wikipedia.org/wiki/Duck_typing), then it might make sense to
think of protocols as things that explicitly define what methods you need in
order to "clearly be a duck".

## Install

`$ npm install -S protoduck`

## Table of Contents

* [Example](#example)
* [Features](#features)
* [Guide](#guide)
  * [Introduction](#introduction)
  * [Defining protocols](#defining-protocols)
  * [Implementations](#protocol-impls)
  * [Multiple dispatch](#multiple-dispatch)
  * [Constraints](#constraints)
* [API](#api)
  * [`define()`](#define)
  * [`proto.impl()`](#impl)

### Example

```javascript
const protoduck = require('protoduck')

// Quackable is a protocol that defines three methods
const Quackable = protoduck.define({
  walk: [],
  talk: [],
  isADuck: [() => true] // default implementation -- it's optional!
})

// `duck` must implement `Quackable` for this function to work. It doesn't
// matter what type or class duck is, as long as it implements Quackable.
function doStuffToDucks (duck) {
  if (!duck.isADuck()) {
    throw new Error('I want a duck!')
  } else {
    console.log(duck.walk())
    console.log(duck.talk())
  }
}

// ...In a different package:
const ducks = require('./ducks')

class Duck () {}

// Implement the protocol on the Duck class.
ducks.Quackable.impl(Duck, {
  walk () { return "*hobble hobble*" }
  talk () { return "QUACK QUACK" }
})

// main.js
ducks.doStuffToDucks(new Duck()) // works!
```

### Features

* Verifies implementations in case methods are missing or wrong ones added
* Helpful, informative error messages
* Optional default method implementations
* Fresh JavaScript Feel™ -- methods work just like native methods when called
* Methods can dispatch on arguments, not just `this` ([multimethods](https://npm.im/genfun))
* Type constraints

### Guide

#### Introduction

Like most Object-oriented languages, JavaScript comes with its own way of
defining methods: You simply add regular `function`s as properties to regular
objects, and when you do `obj.method()`, it calls the right code! ES6/ES2015
further extended this by adding a `class` syntax that allowed this same system
to work with more familiar syntax sugar: `class Foo { method() { ... } }`.

The point of "protocols" is to have a more explicit definitions of what methods
"go together". That is, a protocol is a description of a type of object your
code interacts with. If someone passes an object into your library, and it fits
your defined protocol, the assumption is that the object will work just as well.

Duck typing is a common term for this sort of thing: If it walks like a duck,
and it talks like a duck, then it may as well be a duck, as far as any of our
code is concerned.

Many other languages have similar or identical concepts under different names:
Java's interfaces, Haskell's typeclasses, Rust's traits. Elixir and Clojure both
call them "protocols" as well.

One big advantage to using these protocols is that they let users define their
own versions of some abstraction, without requiring the type to inherit from
another -- protocols are independent of inheritance, even though they're able to
work together with it. If you've ever found yourself in some sort of inheritance
mess, this is exactly the sort of thing you use to escape it.

#### Defining Protocols

The first step to using `protoduck` is to define a protocol. Protocol
definitions look like this:

```javascript
// import the library first!
const protoduck = require('protoduck')

// `Ducklike` is the name of our protocol. It defines what it means for
// something to be "like a duck", as far as our code is concerned.
const Ducklike = protoduck.define([], {
  walk: [], // This says that the protocol requires a "walk" method.
  talk: [] // and ducks also need to talk
  peck: [] // and they can even be pretty scary
})
```

Protocols by themselves don't really *do* anything, they simply define what
methods are included in the protocol, and thus what will need to be implemented.

#### Protocol Impls

The simplest type of definitions for protocols are as regular methods. In this
style, protocols end up working exactly like normal JavaScript methods: they're
added as properties of the target type/object, and we call them using the
`foo.method()` syntax. `this` is accessible inside the methods, as usual.

Implementation syntax is very similar to protocol definitions, using `.impl`:

```javascript
class Dog {}

// Implementing `Ducklike` for `Dog`s
Ducklike.impl(Dog, [], {
  walk () { return '*pads on all fours*' }
  talk () { return 'woof woof. I mean "quack" >_>' }
  peck (victim) { return 'Can I just bite ' + victim + ' instead?...' }
})
```

So now, our `Dog` class has two extra methods: `walk`, and `talk`, and we can
just call them:

```javascript
const pupper = new Dog()

pupper.walk() // *pads on all fours*
pupper.talk() // woof woof. I mean "quack" >_>
pupper.peck('this string') // Can I just bite this string instead?...
```

#### Multiple Dispatch

You may have noticed before that we have these `[]` in various places that don't
seem to have any obvious purpose.

These arrays allow protocols to be implemented not just for a single value of
`this`, but across *all arguments*. That is, you can have methods in these
protocols that use both `this`, and the first argument (or any other arguments)
in order to determine what code to actually execute.

This type of method is called a multimethod, and is one of the differences
between protoduck and the default `class` syntax.

To use it: in the protocol *definitions*, you put matching
strings in different spots where those empty arrays were, and when you
*implement* the protocol, you give the definition the actual types/objects you
want to implement it on, and it takes care of mapping types to the strings you
defined, and making sure the right code is run:

```javascript
const Playful = protoduck.define(['friend'], {// <---\
  playWith: ['friend'] // <------------ these correspond to each other
})

class Cat {}
class Human {}
class Dog {}

// The first protocol is for Cat/Human combination
Playful.impl(Cat, [Human], {
  playWith (human) {
    return '*headbutt* *purr* *cuddle* omg ilu, ' + human.name
  }
})

// And we define it *again* for a different combination
Playful.impl(Cat, [Dog], {
  playWith (dog) {
    return '*scratches* *hisses* omg i h8 u, ' + dog.name
  }
})

// depending on what you call it with, it runs different methods:
const cat = new Cat()
const human = new Human()
const dog = new Dog()

cat.playWith(human) // *headbutt* *purr* *cuddle* omg ilu, Sam
cat.playWith(dog) // *scratches* *hisses* omg i h8 u, Pupper
```

#### Constraints

Sometimes, you want to have all the functionality of a certain protocol, but you
want to add a few requirements or other bits an pieces. Usually, you would have
to define the entire functionality of the "parent" protocol in your own protocol
in order to pull this off. This isn't very DRY and thus prone to errors, missing
or out-of-sync functionality, or other issues. You could also just tell users
"hey, if you implement this, make sure to implement that", but there's no
guarantee they'll know about it, or know which arguments map to what.

This is where constraints come in: You can define a protocol that expects
anything that implements it to *also* implement one or more "parent" protocols.

```javascript
const Show = proto.define({
  // This syntax allows default impls without using arrays.
  toString () {
    return Object.prototype.toString.call(this)
  },
  toJSON () {
    return JSON.stringify(this)
  }
})

const Log = proto.define({
  log () { console.log(this.toString()) }
}, {
  where: Show()
  // Also valid:
  // [Show('this'), Show('a')]
  // [Show('this', ['a', 'b'])]
})

// This fails with an error: must implement Show:
Log.impl(MyThing)

// So derive Show first...
Show.impl(MyThing)
// And now it's ok!
Log.impl(MyThing)
```

### API

#### <a name="define"></a> `define(<types>?, <spec>, <opts>)`

Defines a new protocol on across arguments of types defined by `<types>`, which
will expect implementations for the functions specified in `<spec>`.

If `<types>` is missing, it will be treated the same as if it were an empty
array.

The types in `<spec>` entries must map, by string name, to the type names
specified in `<types>`, or be an empty array if `<types>` is omitted. The types
in `<spec>` will then be used to map between method implementations for the
individual functions, and the provided types in the impl.

Protocols can include an `opts` object as the last argument, with the following
available options:

* `opts.name` `{String}` - The name to use when referring to the protocol.

* `opts.where` `{Array[Constraint]|Constraint}` - Protocol constraints to use.

* `opts.metaobject` - Accepts an object implementing the
  `Protoduck` protocol, which can be used to alter protocol definition
  mechanisms in `protoduck`.

##### Example

```javascript
const Eq = protoduck.define(['a'], {
  eq: ['a']
})
```

#### <a name="impl"></a> `proto.impl(<target>, <types>?, <implementations>?)`

Adds a new implementation to the given protocol across `<types>`.

`<implementations>` must be an object with functions matching the protocol's
API. If given, the types in `<types>` will be mapped to their corresponding
method arguments according to the original protocol definition.

If a protocol is derivable -- that is, all its functions have default impls,
then the `<implementations>` object can be omitted entirely, and the protocol
will be automatically derived for the given `<types>`

##### Example

```javascript
import protoduck from 'protoduck'

// Singly-dispatched protocols
const Show = protoduck.define({
  show: []
})

class Foo {
  constructor (name) {
    this.name = name
  }
}

Show.impl(Foo, {
  show () { return `[object Foo(${this.name})]` }
})

const f = new Foo('alex')
f.show() === '[object Foo(alex)]'
```

```javascript
import protoduck from 'protoduck'

// Multi-dispatched protocols
const Comparable = protoduck.define(['target'], {
  compare: ['target'],
})

class Foo {}
class Bar {}
class Baz {}

Comparable.impl(Foo, [Bar], {
  compare (bar) { return 'bars are ok' }
})

Comparable.impl(Foo, [Baz], {
  compare (baz) { return 'but bazzes are better' }
})

const foo = new Foo()
const bar = new Bar()
const baz = new Baz()

foo.compare(bar) // 'bars are ok'
foo.compare(baz) // 'but bazzes are better'
```
