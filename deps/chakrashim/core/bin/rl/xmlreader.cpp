//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "rl.h"

#include <objbase.h>
#include <XmlLite.h>

#define CHECKHR(x) {hr = x; if (FAILED(hr)) goto CleanUp;}
#define SAFERELEASE(p) {if (p) {(p)->Release(); p = NULL;}}

namespace Xml
{

Node * Node::TopNode;

//-----------------------------------------------------------------------------
//
// Description:
//
//    Constructor for Attribute class.
//
//-----------------------------------------------------------------------------

Attribute::Attribute
(
   Char * name,
   Char * value
)
   : Name(name)
   , Value(value)
   , Next(NULL)
{}

Char *
Attribute::GetValue
(
   const Char * name
)
{
   for (Attribute * p = this; p != NULL; p = p->Next)
   {
      if (strcmp(p->Name, name) == 0)
      {
         return p->Value;
      }
   }

   return NULL;
}

void
Attribute::Dump()
{
   for (Attribute * attr = this;
        attr != NULL;
        attr = attr->Next)
   {
      printf("%s=\"%s\"", attr->Name, attr->Value);
      if (attr->Next != NULL)
      {
         printf(" ");
      }
   }
   printf("\n");
}

//-----------------------------------------------------------------------------
//
// Description:
//
//    Constructor for Node class.
//
//
//-----------------------------------------------------------------------------

Node::Node
(
   Char * name,
   Attribute * attributeList
)
   : Name(name)
   , AttributeList(attributeList)
   , Next(NULL)
   , ChildList(NULL)
   , Data(NULL)
//   , LineNumber(Xml::LineNumber)
{}

Node *
Node::GetChild
(
   const Char * name
)
{
   for (Node * p = this->ChildList; p != NULL; p = p->Next)
   {
      if (strcmp(p->Name, name) == 0)
      {
         return p;
      }
   }

   return NULL;
}

Char *
Node::GetAttributeValue
(
   const Char * name
)
{
   return this->AttributeList->GetValue(name);
}

void
Node::Dump
(
   int indent
)
{
   for (int i = 0; i < indent; i++)
   {
      printf("   ");
   }

   printf("Node %s ", this->Name);
   this->AttributeList->Dump();
   if (this->Data != NULL)
   {
      for (int i = 0; i <= indent; i++)
      {
         printf("   ");
      }
      printf("Data: %s\n", this->Data);
   }
   else
   {
      for (Node * child = this->ChildList;
           child != NULL;
           child = child->Next)
      {
         child->Dump(indent + 1);
      }
   }
}

void
Node::Dump()
{
   this->Dump(0);
}

Char *
ConvertWCHAR
(
   const WCHAR * pWchar
)
{
   int len = ::WideCharToMultiByte(CP_ACP, WC_NO_BEST_FIT_CHARS, pWchar, -1,
      NULL, 0, NULL, NULL);

   Char * newStr = new char[len + 1];

   ::WideCharToMultiByte(CP_ACP, WC_NO_BEST_FIT_CHARS, pWchar, -1,
      newStr, len + 1, NULL, NULL);

   return newStr;
}

HRESULT
ParseAttributes
(
   IXmlReader * pReader,
   Attribute ** ppAttrList
)
{
   HRESULT hr;
   const WCHAR * tmpString;
   Attribute * attrLast = nullptr;

   while (S_OK == (hr = pReader->MoveToNextAttribute()))
   {
      pReader->GetLocalName(&tmpString, nullptr);
      Char * attrName = ConvertWCHAR(tmpString);

      pReader->GetValue(&tmpString, nullptr);
      Char * attrValue = ConvertWCHAR(tmpString);

      Attribute * attrItem = new Attribute(attrName, attrValue);
      if (attrLast != nullptr)
      {
         attrLast->Next = attrItem;
      }
      else
      {
         *ppAttrList = attrItem;
      }
      attrLast = attrItem;
   }

   return hr;
}

HRESULT
ParseNode
(
   IXmlReader * pReader,
   Node ** ppNode
)
{
   HRESULT hr;

   XmlNodeType nodeType;
   Char * nodeName = nullptr;

   Attribute * attrList = nullptr;

   Node * childList = nullptr;
   Node * childLast = nullptr;

   const WCHAR * tmpString;

#define APPEND_CHILD(childNode) \
   if (childLast == nullptr) \
   { \
       childList = childLast = childNode; \
   } \
   else \
   { \
       childLast->Next = childNode; \
       childLast = childNode; \
   }

   // This call won't fail we make sure the reader is positioned at a valid
   // node before ParseNode() is called.
   pReader->GetNodeType(&nodeType);

   do
   {
      switch (nodeType)
      {
      case XmlNodeType_Element:
      {
         bool inOpenElement = nodeName != nullptr;
         if (inOpenElement)
         {
            Node * childNode;
            hr = ParseNode(pReader, &childNode);
            if (hr == S_OK)
            {
               APPEND_CHILD(childNode);
            }
            else
            {
               return hr;
            }
         }
         else
         {
            pReader->GetLocalName(&tmpString, nullptr);
            nodeName = ConvertWCHAR(tmpString);

            hr = ParseAttributes(pReader, &attrList);
            if (FAILED(hr))
            {
               *ppNode = nullptr;
               return hr;
            }

            *ppNode = new Node(nodeName, attrList);

            if (pReader->IsEmptyElement())
            {
               return S_OK;
            }
         }

         break;
      }

      case XmlNodeType_EndElement:
      {
         Node * node = *ppNode;

         // If we have a single child with data called "#text", then pull the data up to this node.
         if (childList != nullptr
             && childList == childLast
             && (childList->Data != nullptr)
             && (_stricmp(childList->Name, "#text") == 0))
         {
            node->Data = childList->Data;
            node->ChildList = nullptr;
         }
         else
         {
            node->ChildList = childList;
         }

         return S_OK;
      }

      case XmlNodeType_Attribute:
         // Need to manually move to attributes when at an element node.
         break;

      case XmlNodeType_CDATA:
      case XmlNodeType_Text:
      {
         pReader->GetValue(&tmpString, nullptr);
         Node * node = new Node("#text", nullptr);
         node->Data = ConvertWCHAR(tmpString);
         APPEND_CHILD(node);

         break;
      }

      case XmlNodeType_Comment:
      case XmlNodeType_DocumentType:
      case XmlNodeType_None:
      case XmlNodeType_ProcessingInstruction:
      case XmlNodeType_Whitespace:
      case XmlNodeType_XmlDeclaration:
         // Ignored.
         break;
      }
   }
   while (S_OK == (hr = pReader->Read(&nodeType)));

   return hr;

#undef APPEND_CHILD
}

HRESULT
ParseXml
(
   IXmlReader * pReader,
   Node ** ppNode
)
{
   HRESULT hr;
   XmlNodeType nodeType;

   // ParseNode() ignores the XML declaration node, so there can be only one
   // top level node.
   if (SUCCEEDED(hr = pReader->Read(&nodeType)))
   {
      return ParseNode(pReader, ppNode);
   }

   return hr;
}

HRESULT
CreateStreamOnHandle
(
   HANDLE handle,
   IStream ** ppStream
)
{
   // Note that this function reads the whole file into memory.
   //
   // There is no API on ARM similar to SHCreateStreamOnFileEx which creates
   // an IStream object that reads a file lazily. Rather than writing our own
   // IStream implementation that does this, we just read the whole file here
   // given that XML files don't get quite large and it should be okay to keep
   // everyting in memory.

   DWORD fileSize, fileSizeHigh, bytesRead;
   HGLOBAL buffer;

   fileSize = GetFileSize(handle, &fileSizeHigh);
   if (fileSize == INVALID_FILE_SIZE || fileSizeHigh != 0)
   {
      return E_FAIL;
   }

   buffer = GlobalAlloc(GPTR, fileSize + 1);
   if (buffer == nullptr)
   {
      return E_FAIL;
   }

   if (!::ReadFile(handle, buffer, fileSize, &bytesRead, nullptr)
       || FAILED(CreateStreamOnHGlobal(buffer, /* fDeleteOnRelease */ true, ppStream)))
   {
      GlobalFree(buffer);
      return E_FAIL;
   }

   return S_OK;
}

Node *
ReadFile
(
   const char * fileName
)
{
   IStream * pStream;
   IXmlReader * pReader;

   HANDLE fileHandle = CreateFile(fileName, FILE_GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
   if (fileHandle == INVALID_HANDLE_VALUE)
   {
      Fatal("Cannot open XML file %s", fileName);
   }

   if (FAILED(CreateStreamOnHandle(fileHandle, &pStream)))
   {
      Fatal("Cannot create stream from file");
   }

   if (FAILED(CreateXmlReader(__uuidof(IXmlReader), (void**) &pReader, nullptr)))
   {
      Fatal("Cannot create XML reader");
   }

   if (FAILED(pReader->SetProperty(XmlReaderProperty_DtdProcessing, DtdProcessing_Prohibit)))
   {
      Fatal("Cannot prohibit DTD processing");
   }

   if (FAILED(pReader->SetInput(pStream)))
   {
      Fatal("Cannot set XML reader input");
   }

   Node * topNode;
   if (FAILED(ParseXml(pReader, &topNode)))
   {
      unsigned int line, linePos;
      pReader->GetLineNumber(&line);
      pReader->GetLinePosition(&linePos);
      fprintf(
         stderr,
         "Error on line %d, position %d in \"%s\".\n",
         line,
         linePos,
         fileName);
      Fatal("Error parsing XML");
   }

   SAFERELEASE(pReader);
   SAFERELEASE(pStream);
   CloseHandle(fileHandle);

   return topNode;
}

}  // namespace Xml
