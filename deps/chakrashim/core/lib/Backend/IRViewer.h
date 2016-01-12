//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

#ifdef IR_VIEWER


class IRtoJSObjectBuilder
{
private:
    static const int BUFFER_LEN = 256;

private:
    /* CreateOpnd helper functions */
    static Js::DynamicObject * CreateIntConstOpnd(Js::ScriptContext *scriptContext, IR::Opnd *opnd);
    static Js::DynamicObject * CreateFloatConstOpnd(Js::ScriptContext *scriptContext, IR::Opnd *opnd);
    static Js::DynamicObject * CreateHelperCallOpnd(Js::ScriptContext *scriptContext, IR::Opnd *opnd);
    static Js::DynamicObject * CreateSymOpnd(Js::ScriptContext *scriptContext, IR::Opnd *opnd);
    static Js::DynamicObject * CreateRegOpnd(Js::ScriptContext *scriptContext, IR::Opnd *opnd);
    static Js::DynamicObject * CreateAddrOpnd(Js::ScriptContext *scriptContext, IR::Opnd *opnd, Func *func);
    static Js::DynamicObject * CreateIndirOpnd(Js::ScriptContext *scriptContext, IR::Opnd *opnd);
    static Js::DynamicObject * CreateLabelOpnd(Js::ScriptContext *scriptContext, IR::Opnd *opnd);
    // TODO (doilij) memref
    // TODO (doilij) regbv

public:
    /* create operands */
    static Js::DynamicObject * CreateOpnd(Js::ScriptContext *scriptContext, IR::Opnd *opnd, Func *func);

    /* utility functions */
    static Js::PropertyId CreateProperty(Js::ScriptContext *scriptContext, const wchar_t *propertyName);
    static void SetProperty(Js::DynamicObject *obj, const wchar_t *propertyName, Js::Var value);
    static void SetProperty(Js::DynamicObject *obj, Js::PropertyId id, Js::Var value);
    static void GetStatementSourceString(__out_ecount(len) wchar_t *buffer, LPCUTF8 sourceBegin, LPCUTF8 sourceEnd, const size_t len);

    /* create instructions */
    static void CreateLabelInstruction(Js::ScriptContext *scriptContext, IR::LabelInstr *inst,
                                       Js::DynamicObject *currObject);
    static void CreateBranchInstruction(Js::ScriptContext *scriptContext, IR::BranchInstr *inst,
                                        Js::DynamicObject *currObject);
    static void CreatePragmaInstruction(Js::ScriptContext *scriptContext, IR::PragmaInstr *inst,
                                        Js::DynamicObject *currObject, Func *func);
    static void CreateDefaultInstruction(Js::ScriptContext *scriptContext, IR::Instr *inst,
                                         Js::DynamicObject *currObject, Func *func);

    static Js::DynamicObject * GetMetadata(Js::ScriptContext *scriptContext);
    static Js::DynamicObject * DumpIRtoJSObject(Func *func, Js::Phase phase);
    static void DumpIRtoGlobalObject(Func *func, Js::Phase phase);
};

#endif /* IR_VIEWER */
