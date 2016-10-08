# tryit

Tiny module wrapping try/catch in JavaScript. 

It's *literally 11 lines of code*, [just read it](tryit.js) that's all the documentation you'll need.


## install

```
npm install tryit
```

## usage 

What you'd normally do:
```js
try {
    dangerousThing();
} catch (e) {
    console.log('something');
}
```

With try-it (all it does is wrap try-catch)
```js
var tryit = require('tryit');

tryit(dangerousThing);
```

You can also handle the error by passing a second function
```js
tryit(dangerousThing, function (e) {
    if (e) {
        console.log('do something');
    }
})
```

The second function follows error-first pattern common in node. So if you pass a callback it gets called in both cases. But will have an error as the first argument if it fails.

## WHAT? WHY DO THIS!? 

Primary motivation is having a clean way to wrap things that might fail, where I don't care if it fails. I just want to try it. 

This includes stuff like blindly reading/parsing stuff from localStorage in the browser. If it's not there or if parsing it fails, that's fine. But I don't want to leave a bunch of empty `catch (e) {}` blocks in the code.

Obviously, this is useful any time you're going to attempt to read some unknown data structure.

In addition, my understanding is that it's hard for JS engines to optimize code in try blocks. By actually passing the code to be executed into a re-used try block, we can avoid having to have more than a single try block in our app. Again, this is not a primary motivation, just a potential side benefit. 


## license

[MIT](http://mit.joreteg.com/)
