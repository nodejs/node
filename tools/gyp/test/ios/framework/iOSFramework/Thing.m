#import "Thing.h"

@interface Thing ()

@end

@implementation Thing

+ (instancetype)thing {
  static Thing* thing = nil;
  static dispatch_once_t onceToken;
  dispatch_once(&onceToken, ^{
      thing = [[[self class] alloc] init];
  });
  return thing;
}

- (void)sayHello {
  NSLog(@"Hello World");
}

@end
