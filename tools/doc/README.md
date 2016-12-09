Here's how the node docs work.

1:1 relationship from `lib/<module>.js` to `doc/api/<module>.md`

Each type of heading has a description block.

```md
## module
<!-- YAML
added: v0.10.0
-->

> Stability: 3 - Stable

description and examples.

### module.property
<!-- YAML
added: v0.10.0
-->

* Type

description of the property.

### module.someFunction(x, y, [z=100])
<!-- YAML
added: v0.10.0
-->

* `x` {String} the description of the string
* `y` {Boolean} Should I stay or should I go?
* `z` {Number} How many zebras to bring.

A description of the function.

### module.someNewFunction(x)
<!-- YAML
added: REPLACEME
-->

* `x` {String} the description of the string

This feature is not in a release yet.

### Event: 'blerg'
<!-- YAML
added: v0.10.0
-->

* Argument: SomeClass object.

Modules don't usually raise events on themselves.  `cluster` is the
only exception.

## Class: SomeClass
<!-- YAML
added: v0.10.0
-->

description of the class.

### Class Method: SomeClass.classMethod(anArg)
<!-- YAML
added: v0.10.0
-->

* `anArg` {Object}  Just an argument
  * `field` {String} anArg can have this field.
  * `field2` {Boolean}  Another field.  Default: `false`.
* Return: {Boolean} `true` if it worked.

Description of the method for humans.

### someClass.nextSibling()
<!-- YAML
added: v0.10.0
-->

* Return: {SomeClass object | null}  The next someClass in line.

### someClass.someProperty
<!-- YAML
added: v0.10.0
-->

* String

The indication of what someProperty is.

### Event: 'grelb'
<!-- YAML
added: v0.10.0
-->

* `isBlerg` {Boolean}

This event is emitted on instances of SomeClass, not on the module itself.
```


* Modules have (description, Properties, Functions, Classes, Examples)
* Properties have (type, description)
* Functions have (list of arguments, description)
* Classes have (description, Properties, Methods, Events)
* Events have (list of arguments, description)
* Methods have (list of arguments, description)
* Properties have (type, description)
