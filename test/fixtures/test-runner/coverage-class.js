'use strict';
const test = require('node:test');

class Animal {
  constructor(name) {
    this.name = name;
  }

  speak() {
    return `${this.name} makes a sound`;
  }

  static create(name) {
    return new Animal(name);
  }
}

class Dog extends Animal {
  static {
    Dog.species = 'Canis familiaris';
  }

  speak() {
    return `${this.name} barks`;
  }
}

// This class is never instantiated — its body should be uncovered.
class UnusedClass {
  doSomething() {
    return 42;
  }
}

test('class coverage', () => {
  const dog = Dog.create('Rex');
  dog.speak();
});
