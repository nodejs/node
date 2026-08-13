---
title: 'Maps (Hidden Classes) in V8'
description: 'How does V8 track and optimize the perceived structure of your objects?'
---

Let's show how V8 builds it's hidden classes. The principal data structures are:

- `Map`: the hidden class itself. It's the first pointer value in an object and therefore allows easy comparison to see if two objects have the same class.
- `DescriptorArray`: The full list of properties that this class has along with information about them. In some cases, the property value is even in this array.
- `TransitionArray`: An array of "edges" from this `Map` to sibling Maps. Each edge is a property name, and should be thought of as "if I were to add a property with this name to the current class, what class would I transition to?"

Because many `Map` objects only have one transition to another one (ie, they are "transitional" maps, only used on the way to something else), V8 doesn't always create a full-blown `TransitionArray` for it. Instead it'll just link directly to this "next" `Map`. The system has to do a bit of spelunking in the `DescriptorArray` of the `Map` being pointed to in order to figure out the name attached to the transition.

This is an extremely rich subject. It's also subject to change, though, if you understand the concepts in this article future change should be incrementally understandable.

## Why have hidden classes?

V8 could do without hidden classes, sure. It would treat each object as a bag of properties. However, a very useful principle would have been left laying around: the principal of intelligent design. V8 surmises that you'll only create so many **different** kinds of objects. And each kind of object will be used in what can eventually be seen to be stereotypical ways. I say "eventually be seen" because the JavaScript language is a scripting language, not a pre-compiled one. So V8 never knows what will come next. To make use of intelligent design (that is, the assumption that there is a mind behind the code coming in), V8 has to watch and wait, letting the sense of structure seep in. The hidden class mechanism is the principal means to do this. Of course, it presupposes a sophisticated listening mechanism, and these are the Inline Caches (ICs) about which much has been written.

So, if you are convinced this is good and necessary work, follow me!

## An example

```javascript
function Peak(name, height, extra) {
  this.name = name;
  this.height = height;
  if (isNaN(extra)) {
    this.experience = extra;
  } else {
    this.prominence = extra;
  }
}

m1 = new Peak("Matterhorn", 4478, 1040);
m2 = new Peak("Wendelstein", 1838, "good");
```

With this code we've already got an interesting map tree from the root map (also known as the initial map) which is attached to the function `Peak`:

<figure>
  <img src="/_img/docs/hidden-classes/drawing-one.svg" width="400" height="480" alt="Hidden class example" loading="lazy">
</figure>

Each blue box is a map, starting with the initial map. This is the map of the object returned if somehow, we managed to run the function `Peak` without adding a single property. The follow-on maps are the ones that result by adding the properties given by the names on the edges between maps. Each map has a list of the properties associated with an object of that map. Furthermore, it describes the exact location of each property. Finally, from one of these maps, say, `Map3` which is the hidden class of the object you'll get if you passed a number for the `extra` argument in `Peak()`, you can follow a back link up all the way to the initial map.

Let's draw it again with this extra information. The annotation (i0), (i1), means in-object field location 0, 1, etc:

<figure>
  <img src="/_img/docs/hidden-classes/drawing-two.svg" width="400" height="480" alt="Hidden class example" loading="lazy">
</figure>

Now, if you spend time examining these maps before you've created at least 7 `Peak` objects, you'll encounter **slack tracking** which can be confusing. I have [another article](https://v8.dev/blog/slack-tracking) about that. Just create 7 more objects and it will be finished. At this point, your Peak objects will have exactly 3 in-object properties, with no possibility to add more directly in the object. Any additional properties will be offloaded to the object's property backing store. It's just an array of property values, whose index comes from the map (Well, technically, from the `DescriptorArray` attached to the map). Let's add a property to `m2` on a new line, and look again at the map tree:

```javascript
m2.cost = "one arm, one leg";
```

<figure>
  <img src="/_img/docs/hidden-classes/drawing-three.svg" width="400" height="480" alt="Hidden class example" loading="lazy">
</figure>

I snuck something in here. Notice that all of the properties are annotated with "const," which means that from V8's point of view, nobody ever changed them since the constructor, so they can be considered constants once they've been initialized. TurboFan (the optimizing compiler) loves this. Say `m2` is referenced as a constant global by a function. Then the lookup of `m2.cost` can be done at compile time, since the field is marked as constant. I'll return to this later in the article.

Notice that property "cost" is marked as `const p0`, which means it's a constant property stored at index zero in the **properties backing store** rather than in the object directly. This is because we have no more room in the object. This information is visible in `%DebugPrint(m2)`:

```
d8> %DebugPrint(m2);
DebugPrint: 0x2f9488e9: [JS_OBJECT_TYPE]
 - map: 0x219473fd <Map(HOLEY_ELEMENTS)> [FastProperties]
 - prototype: 0x2f94876d <Object map = 0x21947335>
 - elements: 0x419421a1 <FixedArray[0]> [HOLEY_ELEMENTS]
 - properties: 0x2f94aecd <PropertyArray[3]> {
    0x419446f9: [String] in ReadOnlySpace: #name: 0x237125e1
        <String[11]: #Wendelstein> (const data field 0)
    0x23712581: [String] in OldSpace: #height:
        1838 (const data field 1)
    0x23712865: [String] in OldSpace: #experience: 0x237125f9
        <String[4]: #good> (const data field 2)
    0x23714515: [String] in OldSpace: #cost: 0x23714525
        <String[16]: #one arm, one leg>
        (const data field 3) properties[0]
 }
 ...
{name: "Wendelstein", height: 1, experience: "good", cost: "one arm, one leg"}
d8>
```

You can see that we have 4 properties, all marked as const. The first 3 in the object, and the last in `properties[0]` which means the first slot of the properties backing store. We can look at that:

```
d8> %DebugPrintPtr(0x2f94aecd)
DebugPrint: 0x2f94aecd: [PropertyArray]
 - map: 0x41942be9 <Map>
 - length: 3
 - hash: 0
         0: 0x23714525 <String[16]: #one arm, one leg>
       1-2: 0x41942329 <undefined>
```

The extra properties are there just in case you decide to add more all of a sudden.

## The real structure

There are different things we could do at this point, but since you must really like V8, having read this far, I'd like to try drawing the real data structures we use, the ones mentioned at the beginning of `Map`, `DescriptorArray`, and `TransitionArray`. Now that you have some idea of the hidden class concept being built up behind the scenes, you may as well bind your thinking more closely to the code through the right names and structures. Let me try and reproduce that last figure in V8's representation. First I'm going to draw the **DescriptorArrays**, which hold the list of properties for a given Map. These arrays can be shared -- the key to that is that the Map itself knows how many properties it is allowed to look at in the DescriptorArray. Since the properties are in the order they were added in time, these arrays can be shared by several maps. See:

<figure>
  <img src="/_img/docs/hidden-classes/drawing-four.svg" width="600" height="480" alt="Hidden class example" loading="lazy">
</figure>

Notice that **Map1**, **Map2**, and **Map3** all point to **DescriptorArray1**. The number next to the "descriptors" field in each Map indicates how many fields over in the DescriptorArray belong to the Map. So **Map1**, which only knows about the "name" property, looks only at the first property listed in **DescriptorArray1**. Whereas **Map2** has two properties, "name" and "height." So it looks at the first and second items in **DescriptorArray1** (name and height). This kind of sharing saves a lot of space.

Naturally, we can't share where there is a split. There is a transition from Map2 over to Map4 if the "experience" property is added, and over to Map3 if the "prominence" property is added. You can see Map4 and Map5 sharing DescriptorArray2 in the same way that DescriptorArray1 was shared among three Maps.

The only thing missing from our "true to life" diagram is the `TransitionArray` which is still metaphorical at this point. Let's change that. I took the liberty of removing the **back pointer** lines, which cleans things up a bit. Just remember that from any Map in the tree, you can walk up the tree, too.

<figure>
  <img src="/_img/docs/hidden-classes/drawing-five.svg" width="600" height="480" alt="Hidden class example" loading="lazy">
</figure>

The diagram rewards study. **Question: what would happen if a new property "rating" was added after "name" instead of going on to "height" and other properties?**

**Answer**: Map1 would get a real **TransitionArray** so as to keep track of the bifurcation. If property *height* is added, we should transition to **Map2**. However, if property *rating* is added, we should go to a new map, **Map6**. This map would need a new DescriptorArray that mentions *name* and *rating*. The object has extra free slots at this point in the object (only one of three is used), so property *rating* will be given one of those slots.

*I checked my answer with the help of `%DebugPrintPtr()`, and drew the following:*

<figure>
  <img src="/_img/docs/hidden-classes/drawing-six.svg" width="500" height="480" alt="Hidden class example" loading="lazy">
</figure>

No need to beg me to stop, I see that this is the upper limit of such diagrams! But I think you can get a sense of how the parts move. Just imagine if after adding this ersatz property *rating*, we continued on with *height*, *experience* and *cost*. Well, we'd have to create maps **Map7**, **Map8** and **Map9**. Because we insisted on adding this property in the middle of an established chain of maps, we will duplicate much structure. I don't have the heart to make that drawing -- though if you send it to me I will add it to this document :).

I used the handy [DreamPuf](https://dreampuf.github.io/GraphvizOnline) project to make the diagrams easily. Here is a [link](https://dreampuf.github.io/GraphvizOnline/#digraph%20G%20%7B%0A%0A%20%20node%20%5Bfontname%3DHelvetica%2C%20shape%3D%22record%22%2C%20fontcolor%3D%22white%22%2C%20style%3Dfilled%2C%20color%3D%22%233F53FF%22%5D%0A%20%20edge%20%5Bfontname%3DHelvetica%5D%0A%20%20%0A%20%20Map0%20%5Blabel%3D%22%7B%3Ch%3E%20Map0%20%7C%20%3Cd%3E%20descriptors%20(0)%20%7C%20%3Ct%3E%20transitions%20(1)%7D%22%5D%3B%0A%20%20Map1%20%5Blabel%3D%22%7B%3Ch%3E%20Map1%20%7C%20%3Cd%3E%20descriptors%20(1)%20%7C%20%3Ct%3E%20transitions%20(1)%7D%22%5D%3B%0A%20%20Map2%20%5Blabel%3D%22%7B%3Ch%3E%20Map2%20%7C%20%3Cd%3E%20descriptors%20(2)%20%7C%20%3Ct%3E%20transitions%20(2)%7D%22%5D%3B%0A%20%20Map3%20%5Blabel%3D%22%7B%3Ch%3E%20Map3%20%7C%20%3Cd%3E%20descriptors%20(3)%20%7C%20%3Ct%3E%20transitions%20(0)%7D%22%5D%0A%20%20Map4%20%5Blabel%3D%22%7B%3Ch%3E%20Map4%20%7C%20%3Cd%3E%20descriptors%20(3)%20%7C%20%3Ct%3E%20transitions%20(1)%7D%22%5D%0A%20%20Map5%20%5Blabel%3D%22%7B%3Ch%3E%20Map5%20%7C%20%3Cd%3E%20descriptors%20(4)%20%7C%20%3Ct%3E%20transitions%20(0)%7D%22%5D%0A%20%20Map6%20%5Blabel%3D%22%7B%3Ch%3E%20Map6%20%7C%20%3Cd%3E%20descriptors%20(2)%20%7C%20%3Ct%3E%20transitions%20(0)%7D%22%5D%3B%0A%20%20Map0%3At%20-%3E%20Map1%20%5Blabel%3D%22name%20(inferred)%22%5D%3B%0A%20%20%0A%20%20Map4%3At%20-%3E%20Map5%20%5Blabel%3D%22cost%20(inferred)%22%5D%3B%0A%20%20%0A%20%20%2F%2F%20Create%20the%20descriptor%20arrays%0A%20%20node%20%5Bfontname%3DHelvetica%2C%20shape%3D%22record%22%2C%20fontcolor%3D%22black%22%2C%20style%3Dfilled%2C%20color%3D%22%23FFB34D%22%5D%3B%0A%20%20%0A%20%20DA0%20%5Blabel%3D%22%7BDescriptorArray0%20%7C%20(empty)%7D%22%5D%0A%20%20Map0%3Ad%20-%3E%20DA0%3B%0A%20%20DA1%20%5Blabel%3D%22%7BDescriptorArray1%20%7C%20name%20(const%20i0)%20%7C%20height%20(const%20i1)%20%7C%20prominence%20(const%20i2)%7D%22%5D%3B%0A%20%20Map1%3Ad%20-%3E%20DA1%3B%0A%20%20Map2%3Ad%20-%3E%20DA1%3B%0A%20%20Map3%3Ad%20-%3E%20DA1%3B%0A%20%20%0A%20%20DA2%20%5Blabel%3D%22%7BDescriptorArray2%20%7C%20name%20(const%20i0)%20%7C%20height%20(const%20i1)%20%7C%20experience%20(const%20i2)%20%7C%20cost%20(const%20p0)%7D%22%5D%3B%0A%20%20Map4%3Ad%20-%3E%20DA2%3B%0A%20%20Map5%3Ad%20-%3E%20DA2%3B%0A%20%20%0A%20%20DA3%20%5Blabel%3D%22%7BDescriptorArray3%20%7C%20name%20(const%20i0)%20%7C%20rating%20(const%20i1)%7D%22%5D%3B%0A%20%20Map6%3Ad%20-%3E%20DA3%3B%0A%20%20%0A%20%20%2F%2F%20Create%20the%20transition%20arrays%0A%20%20node%20%5Bfontname%3DHelvetica%2C%20shape%3D%22record%22%2C%20fontcolor%3D%22white%22%2C%20style%3Dfilled%2C%20color%3D%22%23B3813E%22%5D%3B%0A%20%20TA0%20%5Blabel%3D%22%7BTransitionArray0%20%7C%20%3Ca%3E%20experience%20%7C%20%3Cb%3E%20prominence%7D%22%5D%3B%0A%20%20Map2%3At%20-%3E%20TA0%3B%0A%20%20TA0%3Aa%20-%3E%20Map4%3Ah%3B%0A%20%20TA0%3Ab%20-%3E%20Map3%3Ah%3B%0A%20%20%0A%20%20TA1%20%5Blabel%3D%22%7BTransitionArray1%20%7C%20%3Ca%3E%20rating%20%7C%20%3Cb%3E%20height%7D%22%5D%3B%0A%20%20Map1%3At%20-%3E%20TA1%3B%0A%20%20TA1%3Ab%20-%3E%20Map2%3B%0A%20%20TA1%3Aa%20-%3E%20Map6%3B%0A%7D) to the previous diagram.

## TurboFan and const properties

Thus far, all these fields are marked in the `DescriptorArray` as `const`. Let's play with this. Run the following code on a debug build:

```javascript
// run as:
// d8 --allow-natives-syntax --no-lazy-feedback-allocation --code-comments --print-opt-code
function Peak(name, height) {
  this.name = name;
  this.height = height;
}

let m1 = new Peak("Matterhorn", 4478);
m2 = new Peak("Wendelstein", 1838);

// Make sure slack tracking finishes.
for (let i = 0; i < 7; i++) new Peak("blah", i);

m2.cost = "one arm, one leg";
function foo(a) {
  return m2.cost;
}

foo(3);
foo(3);
%OptimizeFunctionOnNextCall(foo);
foo(3);
```

You'll get a printout of optimized function `foo()`. The code is very short. You'll see at the end of the function:

```
...
40  mov eax,0x2a812499          ;; object: 0x2a812499 <String[16]: #one arm, one leg>
45  mov esp,ebp
47  pop ebp
48  ret 0x8                     ;; return "one arm, one leg"!
```

TurboFan, being a cheeky devil, just directly inserted the value of `m2.cost`. Well how do you like that!

Of course, after that last call to `foo()` you could insert this line:

```javascript
m2.cost = "priceless";
```

What do you think will happen? One thing for sure, we can't let `foo()` stay as it is. It would return the wrong answer. Re-run the program, but add flag `--trace-deopt` so you'll be told when optimized code is removed from the system. After the printout of the optimized `foo()`, you'll see these lines:

```
[marking dependent code 0x5c684901 0x21e525b9 <SharedFunctionInfo foo> (opt #0) for deoptimization,
    reason: field-const]
[deoptimize marked code in all contexts]
```

Wow.

<figure>
  <img src="/_img/docs/hidden-classes/i_like_it_a_lot.gif" width="440" height="374" alt="I like it a lot" loading="lazy">
</figure>

If you force re-optimization you'll get code that is not quite as good, but still benefits greatly from the Map structure we've been describing. Remember from our diagrams that property *cost* is the first property in
the properties backing store for an object. Well, it may have lost it's const designation, but we still have it's address. Basically, in an object with map **Map5**, which we'll certainly verify that global variable `m2` still has, we only have to--

1. load the properties backing store, and
2. read out the first array element.

Let's see that. Add this code below the last line:

```javascript
// Force reoptimization of foo().
foo(3);
%OptimizeFunctionOnNextCall(foo);
foo(3);
```

Now have a look at the code produced:

```
...
40  mov ecx,0x42cc8901          ;; object: 0x42cc8901 <Peak map = 0x3d5873ad>
45  mov ecx,[ecx+0x3]           ;; Load the properties backing store
48  mov eax,[ecx+0x7]           ;; Get the first element.
4b  mov esp,ebp
4d  pop ebp
4e  ret 0x8                     ;; return it in register eax!
```

Why heck. That's exactly what we said should happen. Perhaps we are beginning to Know.

TurboFan is also smart enough to deoptimize if variable `m2` ever changes to a different class. You can watch the latest optimized code deoptimize again with something droll like:

```javascript
m2 = 42;  // heh.
```

## Where to go from here

Many options. Map migration. Dictionary mode (aka "slow mode"). Lots to explore in this area and I hope you enjoy yourself as much as I do -- thanks for reading!

## See Also
- [Hidden Classes and Inline Caches](hidden-classes-and-ics.md)
