/* Copyright (c) 2012-2017 The ANTLR Project. All rights reserved.
 * Use of this file is governed by the BSD 3-clause license that
 * can be found in the LICENSE.txt file in the project root.
 */

#include "Parser.h"

#include "InputMismatchException.h"

using namespace antlr4;

InputMismatchException::InputMismatchException(Parser* recognizer)
    : RecognitionException(recognizer, recognizer->getInputStream(),
                           recognizer->getContext(),
                           recognizer->getCurrentToken()) {}

InputMismatchException::~InputMismatchException() {}
