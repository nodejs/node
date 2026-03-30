// Copyright 2013 the V8 project authors. All rights reserved.
// Copyright (C) 2005, 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
// 1.  Redistributions of source code must retain the above copyright
//     notice, this list of conditions and the following disclaimer.
// 2.  Redistributions in binary form must reproduce the above copyright
//     notice, this list of conditions and the following disclaimer in the
//     documentation and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
// EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
// WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
// DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
// (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
// LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
// ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

description(
"This test checks for a crash in sort() that was seen on a particular input data set."
);

function natcompare(a, b) {
    if (a == b)
        return 0;
    return (a < b) ? -1 : 1;
}

SubwayData = [
"23rd St-Broadway ",
"45 Road-Court Sq",
"LIC-Court Sq",
"LIC-Court Sq",
"23rd St-Park Ave S",
"241st St",
"242nd St",
"25th Ave",
"25th St",
"28th St-7th Ave",
"28th St-Broadway",
"28th St-Park Ave S",
"2nd Ave-Houston St",
"30th Ave",
"33rd St",
"33rd St-Park Ave",
"34th St-6th Ave",
"34th St-7th Ave",
"34th St-8th Ave",
"36th Ave",
"36th St",
"36th St",
"39th Ave",
"3rd Ave-138th St",
"3rd Ave-149th St",
"3rd Ave-14th St",
"40th St",
"42nd St-5th Ave-6th Ave",
"42nd St-5th Ave-6th Ave",
"45th St",
"46th St",
"46th St",
"47-50th Sts-Rockefeller Center",
"49th St-7th Ave",
"50th St-New Utrecht Ave",
"9th Ave",
"90th St-Elmhurst Ave",
"96th St",
"96th St",
"96th St",
"9th St-4th Ave",
"Alabama Ave",
"Allerton Ave",
"Aqueduct-North Conduit Ave",
"Astor Place",
"Astoria Blvd",
"Atlantic Ave",
"Atlantic Ave-Pacific St",
"Ave H",
"Ave N",
"Ave P",
"Ave U",
"Ave U",
"Ave U",
"Ave X",
"Bay Pkwy",
"Bay Pkwy",
"Bay Pkwy-22nd Ave",
"Bay Ridge Ave",
"Baychester Ave",
"Beach 105th St",
"Beach 25th St",
"Beach 36th St",
"Beach 44th St",
"Beach 60th St",
"Beach 67th St",
"Beach 90th St",
"Beach 98th St",
"Bedford Ave",
"Bedford Park Blvd",
"Broadway",
"Broadway",
"Bronx Park East",
"Brook Ave",
"Buhre Ave",
"Burke Ave",
"Burnside Ave",
"Bushwick Ave",
"Uptown Bleecker St-Lafayette St",
"Downtown Bleecker St-Lafayette St",
"Canal Street",
"Canal Street",
"Canal Street",
"Canal-Church Sts"
];

SubwayData.sort(natcompare)
