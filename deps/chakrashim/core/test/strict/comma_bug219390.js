//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

(function(){ "use strict", delete z; })(); // Ignore use strict directive with a comma
(function(){ "invalid" + "use strict"; delete z; })();
(function(){ delete z; "use strict"; delete z; })();
(function(){ 0123; "use strict"; delete z; })();
try {
    eval("(function(eval){ \"use strict\" })();");
} catch (e) {
    WScript.Echo(e);
}
(function(){ "use strict" + "use strict"; delete z; });
(function(){ "use strict", "use strict"; delete z;});

WScript.Echo("Pass");