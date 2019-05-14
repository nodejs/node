// Copyright 2012 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

// Long string to trigger caching.
var string =
"Friends, Romans, countrymen, lend me your ears!  \
 I come to bury Caesar, not to praise him.        \
 The evil that men do lives after them,           \
 The good is oft interred with their bones;       \
 So let it be with Caesar. The noble Brutus       \
 Hath told you Caesar was ambitious;              \
 If it were so, it was a grievous fault,          \
 And grievously hath Caesar answer'd it.          \
 Here, under leave of Brutus and the rest-        \
 For Brutus is an honorable man;                  \
 So are they all, all honorable men-              \
 Come I to speak in Caesar's funeral.             \
 He was my friend, faithful and just to me;       \
 But Brutus says he was ambitious,                \
 And Brutus is an honorable man.                  \
 He hath brought many captives home to Rome,      \
 Whose ransoms did the general coffers fill.      \
 Did this in Caesar seem ambitious?               \
 When that the poor have cried, Caesar hath wept; \
 Ambition should be made of sterner stuff:        \
 Yet Brutus says he was ambitious,                \
 And Brutus is an honorable man.                  \
 You all did see that on the Lupercal             \
 I thrice presented him a kingly crown,           \
 Which he did thrice refuse. Was this ambition?   \
 Yet Brutus says he was ambitious,                \
 And sure he is an honorable man.                 \
 I speak not to disprove what Brutus spoke,       \
 But here I am to speak what I do know.           \
 You all did love him once, not without cause;    \
 What cause withholds you then to mourn for him?  \
 O judgement, thou art fled to brutish beasts,    \
 And men have lost their reason. Bear with me;    \
 My heart is in the coffin there with Caesar,     \
 And I must pause till it come back to me.";

var replaced = string.replace(/\b\w+\b/g, function() { return "foo"; });
for (var i = 0; i < 3; i++) {
  assertEquals(replaced,
               string.replace(/\b\w+\b/g, function() { return "foo"; }));
}

// Check that the result is in a COW array.
var words = string.split(" ");
assertEquals("Friends,", words[0]);
words[0] = "Enemies,";
words = string.split(" ");
assertEquals("Friends,", words[0]);
