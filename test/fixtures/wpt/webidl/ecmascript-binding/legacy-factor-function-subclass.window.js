"use strict";

test(() => {
  class CustomImage extends Image {}
  var instance = new CustomImage();

  assert_equals(
    Object.getPrototypeOf(instance), CustomImage.prototype,
    "Object.getPrototypeOf(instance) === CustomImage.prototype");

  assert_true(instance instanceof CustomImage, "instance instanceof CustomImage");
  assert_true(instance instanceof HTMLImageElement, "instance instanceof HTMLImageElement");
}, "[LegacyFactoryFunction] can be subclassed and correctly handles NewTarget");
