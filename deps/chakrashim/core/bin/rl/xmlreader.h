//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//
// Description:
//
//   Simple C++ Xml Reader classes.
//
// Remarks:
//
//-----------------------------------------------------------------------------

namespace Xml
{


// May want Unicode someday.

typedef char Char;


class Attribute
{

public:

   Attribute(Char * name, Char * value);

   Char * GetValue(const Char * name);

   void Dump();

public:

   Attribute * Next;
   Char * Name;
   Char * Value;
};

class Node
{

public:

   Node() {}
   Node(Char * name, Attribute * attributeList);

   Node * GetChild(const Char * name);
   Char * GetAttributeValue(const Char * name);

   void Dump(int indent);
   void Dump();

public:

   Node * Next;
   Node * ChildList;
   Attribute * AttributeList;
   Char * Name;
   Char * Data;
   int LineNumber;

   static Node * TopNode;
};


Node * ReadFile(const char * fileName);


} // namespace Xml
