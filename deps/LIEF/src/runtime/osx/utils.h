#pragma once
#import <Foundation/Foundation.h>
#include <string>

namespace LIEF::runtime::osx {
std::string NSStringToUTF8(NSString* nsstring);
}
