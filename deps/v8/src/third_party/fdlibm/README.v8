Name: Freely Distributable LIBM 
Short Name: fdlibm
URL: http://www.netlib.org/fdlibm/
Version: 5.3 
License: Freely Distributable.
License File: LICENSE.
Security Critical: yes.
License Android Compatible: yes.

Description:
This is used to provide a accurate implementation for trigonometric functions
used in V8.

Local Modifications:
For the use in V8, fdlibm has been reduced to include only sine, cosine and
tangent.  To make inlining into generated code possible, a large portion of
that has been translated to Javascript.  The rest remains in C, but has been
refactored and reformatted to interoperate with the rest of V8.
