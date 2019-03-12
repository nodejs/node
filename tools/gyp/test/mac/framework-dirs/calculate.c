/* Copyright (c) 2012 Google Inc. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

int CalculatePerformExpression(char* expr,
                               int significantDigits,
                               int flags,
                               char* answer);

int main() {
  char buffer[1024];
  return CalculatePerformExpression("42", 1, 0, buffer);
}

