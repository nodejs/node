//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function write(v) { WScript.Echo(v + ""); }

var count = 0; 
var total = 0;

var x = null;

try { write(x.y);  } catch (e) { write(count++ + " " + e.message); } total++;
try { x.y = 5;     } catch (e) { write(count++ + " " + e.message); } total++;
try { delete x.y;  } catch (e) { write(count++ + " " + e.message); } total++;

try { write(x[6]); } catch (e) { write(count++ + " " + e.message); } total++;
try { x[6] = 7;    } catch (e) { write(count++ + " " + e.message); } total++;
try { delete x[6]  } catch (e) { write(count++ + " " + e.message); } total++;

x = undefined;

try { write(x.y);  } catch (e) { write(count++ + " " + e.message); } total++;
try { x.y = 5;     } catch (e) { write(count++ + " " + e.message); } total++;
try { delete x.y;  } catch (e) { write(count++ + " " + e.message); } total++;

try { write(x[6]); } catch (e) { write(count++ + " " + e.message); } total++;
try { x[6] = 7;    } catch (e) { write(count++ + " " + e.message); } total++;
try { delete x[6]  } catch (e) { write(count++ + " " + e.message); } total++;

var a = [ null ];

try { write(a[0].y);  } catch (e) { write(count++ + " " + e.message); } total++;
try { a[0].y = 5;     } catch (e) { write(count++ + " " + e.message); } total++;
try { delete a[0].y;  } catch (e) { write(count++ + " " + e.message); } total++;

try { write(a[0][6]); } catch (e) { write(count++ + " " + e.message); } total++;
try { a[0][6] = 7;    } catch (e) { write(count++ + " " + e.message); } total++;
try { delete a[0][6]  } catch (e) { write(count++ + " " + e.message); } total++;

a = [ undefined ];

try { write(a[0].y);  } catch (e) { write(count++ + " " + e.message); } total++;
try { a[0].y = 5;     } catch (e) { write(count++ + " " + e.message); } total++;
try { delete a[0].y;  } catch (e) { write(count++ + " " + e.message); } total++;

try { write(a[0][6]); } catch (e) { write(count++ + " " + e.message); } total++;
try { a[0][6] = 7;    } catch (e) { write(count++ + " " + e.message); } total++;
try { delete a[0][6]  } catch (e) { write(count++ + " " + e.message); } total++;

var o = { z : null }

try { write(o.z.y);   } catch (e) { write(count++ + " " + e.message); } total++;
try { o.z.y = 5;      } catch (e) { write(count++ + " " + e.message); } total++;
try { delete o.z.y;   } catch (e) { write(count++ + " " + e.message); } total++;

try { write(o.z[6]);  } catch (e) { write(count++ + " " + e.message); } total++;
try { o.z[6] = 7;     } catch (e) { write(count++ + " " + e.message); } total++;
try { delete o.z[6]   } catch (e) { write(count++ + " " + e.message); } total++;

o = { z : undefined }

try { write(o.z.y);   } catch (e) { write(count++ + " " + e.message); } total++;
try { o.z.y = 5;      } catch (e) { write(count++ + " " + e.message); } total++;
try { delete o.z.y;   } catch (e) { write(count++ + " " + e.message); } total++;

try { write(o.z[6]);  } catch (e) { write(count++ + " " + e.message); } total++;
try { o.z[6] = 7;     } catch (e) { write(count++ + " " + e.message); } total++;
try { delete o.z[6]   } catch (e) { write(count++ + " " + e.message); } total++;

write("count: " + count + " total: " + total);