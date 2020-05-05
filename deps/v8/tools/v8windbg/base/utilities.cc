// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/v8windbg/base/utilities.h"

#include <comutil.h>
#include <oleauto.h>

#include <vector>

namespace {

HRESULT BoxObject(IDataModelManager* p_manager, IUnknown* p_object,
                  ModelObjectKind kind, IModelObject** pp_model_object) {
  *pp_model_object = nullptr;

  VARIANT vt_val;
  vt_val.vt = VT_UNKNOWN;
  vt_val.punkVal = p_object;

  HRESULT hr = p_manager->CreateIntrinsicObject(kind, &vt_val, pp_model_object);
  return hr;
}

}  // namespace

HRESULT CreateProperty(IDataModelManager* p_manager,
                       IModelPropertyAccessor* p_property,
                       IModelObject** pp_property_object) {
  return BoxObject(p_manager, p_property, ObjectPropertyAccessor,
                   pp_property_object);
}

HRESULT CreateMethod(IDataModelManager* p_manager, IModelMethod* p_method,
                     IModelObject** pp_method_object) {
  return BoxObject(p_manager, p_method, ObjectMethod, pp_method_object);
}

HRESULT UnboxProperty(IModelObject* object, IModelPropertyAccessor** result) {
  ModelObjectKind kind = (ModelObjectKind)-1;
  RETURN_IF_FAIL(object->GetKind(&kind));
  if (kind != ObjectPropertyAccessor) return E_FAIL;
  _variant_t variant;
  RETURN_IF_FAIL(object->GetIntrinsicValue(&variant));
  if (variant.vt != VT_UNKNOWN) return E_FAIL;
  WRL::ComPtr<IModelPropertyAccessor> accessor;
  RETURN_IF_FAIL(WRL::ComPtr<IUnknown>(variant.punkVal).As(&accessor));
  *result = accessor.Detach();
  return S_OK;
}

HRESULT CreateTypedIntrinsic(uint64_t value, IDebugHostType* type,
                             IModelObject** result) {
  // Figure out what kind of VARIANT we need to make.
  IntrinsicKind kind;
  VARTYPE carrier;
  RETURN_IF_FAIL(type->GetIntrinsicType(&kind, &carrier));

  VARIANT vt_val;
  switch (carrier) {
    case VT_BOOL:
      vt_val.boolVal = value ? VARIANT_TRUE : VARIANT_FALSE;
      break;
    case VT_I1:
      vt_val.cVal = static_cast<int8_t>(value);
      break;
    case VT_UI1:
      vt_val.bVal = static_cast<uint8_t>(value);
      break;
    case VT_I2:
      vt_val.iVal = static_cast<int16_t>(value);
      break;
    case VT_UI2:
      vt_val.uiVal = static_cast<uint16_t>(value);
      break;
    case VT_INT:
      vt_val.intVal = static_cast<int>(value);
      break;
    case VT_UINT:
      vt_val.uintVal = static_cast<unsigned int>(value);
      break;
    case VT_I4:
      vt_val.lVal = static_cast<int32_t>(value);
      break;
    case VT_UI4:
      vt_val.ulVal = static_cast<uint32_t>(value);
      break;
    case VT_INT_PTR:
      vt_val.llVal = static_cast<intptr_t>(value);
      break;
    case VT_UINT_PTR:
      vt_val.ullVal = static_cast<uintptr_t>(value);
      break;
    case VT_I8:
      vt_val.llVal = static_cast<int64_t>(value);
      break;
    case VT_UI8:
      vt_val.ullVal = static_cast<uint64_t>(value);
      break;
    default:
      return E_FAIL;
  }
  vt_val.vt = carrier;
  return sp_data_model_manager->CreateTypedIntrinsicObject(&vt_val, type,
                                                           result);
}

HRESULT CreateULong64(ULONG64 value, IModelObject** pp_int) {
  HRESULT hr = S_OK;
  *pp_int = nullptr;

  VARIANT vt_val;
  vt_val.vt = VT_UI8;
  vt_val.ullVal = value;

  hr = sp_data_model_manager->CreateIntrinsicObject(ObjectIntrinsic, &vt_val,
                                                    pp_int);
  return hr;
}

HRESULT UnboxULong64(IModelObject* object, ULONG64* value, bool convert) {
  ModelObjectKind kind = (ModelObjectKind)-1;
  RETURN_IF_FAIL(object->GetKind(&kind));
  if (kind != ObjectIntrinsic) return E_FAIL;
  _variant_t variant;
  RETURN_IF_FAIL(object->GetIntrinsicValue(&variant));
  if (convert) {
    RETURN_IF_FAIL(VariantChangeType(&variant, &variant, 0, VT_UI8));
  }
  if (variant.vt != VT_UI8) return E_FAIL;
  *value = variant.ullVal;
  return S_OK;
}

HRESULT CreateInt32(int value, IModelObject** pp_int) {
  HRESULT hr = S_OK;
  *pp_int = nullptr;

  VARIANT vt_val;
  vt_val.vt = VT_I4;
  vt_val.intVal = value;

  hr = sp_data_model_manager->CreateIntrinsicObject(ObjectIntrinsic, &vt_val,
                                                    pp_int);
  return hr;
}

HRESULT CreateUInt32(uint32_t value, IModelObject** pp_int) {
  HRESULT hr = S_OK;
  *pp_int = nullptr;

  VARIANT vt_val;
  vt_val.vt = VT_UI4;
  vt_val.uintVal = value;

  hr = sp_data_model_manager->CreateIntrinsicObject(ObjectIntrinsic, &vt_val,
                                                    pp_int);
  return hr;
}

HRESULT CreateBool(bool value, IModelObject** pp_val) {
  HRESULT hr = S_OK;
  *pp_val = nullptr;

  VARIANT vt_val;
  vt_val.vt = VT_BOOL;
  vt_val.boolVal = value;

  hr = sp_data_model_manager->CreateIntrinsicObject(ObjectIntrinsic, &vt_val,
                                                    pp_val);
  return hr;
}

HRESULT CreateNumber(double value, IModelObject** pp_val) {
  HRESULT hr = S_OK;
  *pp_val = nullptr;

  VARIANT vt_val;
  vt_val.vt = VT_R8;
  vt_val.dblVal = value;

  hr = sp_data_model_manager->CreateIntrinsicObject(ObjectIntrinsic, &vt_val,
                                                    pp_val);
  return hr;
}

HRESULT CreateString(std::u16string value, IModelObject** pp_val) {
  HRESULT hr = S_OK;
  *pp_val = nullptr;

  VARIANT vt_val;
  vt_val.vt = VT_BSTR;
  vt_val.bstrVal =
      ::SysAllocString(reinterpret_cast<const OLECHAR*>(value.c_str()));

  hr = sp_data_model_manager->CreateIntrinsicObject(ObjectIntrinsic, &vt_val,
                                                    pp_val);
  return hr;
}

HRESULT UnboxString(IModelObject* object, BSTR* value) {
  ModelObjectKind kind = (ModelObjectKind)-1;
  RETURN_IF_FAIL(object->GetKind(&kind));
  if (kind != ObjectIntrinsic) return E_FAIL;
  _variant_t variant;
  RETURN_IF_FAIL(object->GetIntrinsicValue(&variant));
  if (variant.vt != VT_BSTR) return E_FAIL;
  *value = variant.Detach().bstrVal;
  return S_OK;
}

HRESULT GetModelAtIndex(WRL::ComPtr<IModelObject>& sp_parent,
                        WRL::ComPtr<IModelObject>& sp_index,
                        IModelObject** p_result) {
  WRL::ComPtr<IIndexableConcept> sp_indexable_concept;
  RETURN_IF_FAIL(sp_parent->GetConcept(__uuidof(IIndexableConcept),
                                       &sp_indexable_concept, nullptr));

  std::vector<IModelObject*> p_indexers{sp_index.Get()};
  return sp_indexable_concept->GetAt(sp_parent.Get(), 1, p_indexers.data(),
                                     p_result, nullptr);
}

HRESULT GetCurrentThread(WRL::ComPtr<IDebugHostContext>& sp_host_context,
                         IModelObject** p_current_thread) {
  WRL::ComPtr<IModelObject> sp_boxed_context, sp_root_namespace;
  WRL::ComPtr<IModelObject> sp_debugger, sp_sessions, sp_processes, sp_threads;
  WRL::ComPtr<IModelObject> sp_curr_session, sp_curr_process;

  RETURN_IF_FAIL(BoxObject(sp_data_model_manager.Get(), sp_host_context.Get(),
                           ObjectContext, &sp_boxed_context));
  RETURN_IF_FAIL(sp_data_model_manager->GetRootNamespace(&sp_root_namespace));
  RETURN_IF_FAIL(
      sp_root_namespace->GetKeyValue(L"Debugger", &sp_debugger, nullptr));
  RETURN_IF_FAIL(sp_debugger->GetKeyValue(L"Sessions", &sp_sessions, nullptr));
  RETURN_IF_FAIL(
      GetModelAtIndex(sp_sessions, sp_boxed_context, &sp_curr_session));
  RETURN_IF_FAIL(
      sp_curr_session->GetKeyValue(L"Processes", &sp_processes, nullptr));
  RETURN_IF_FAIL(
      GetModelAtIndex(sp_processes, sp_boxed_context, &sp_curr_process));
  RETURN_IF_FAIL(
      sp_curr_process->GetKeyValue(L"Threads", &sp_threads, nullptr));
  return GetModelAtIndex(sp_threads, sp_boxed_context, p_current_thread);
}
