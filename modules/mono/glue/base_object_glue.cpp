// SPDX-FileCopyrightText: 2023 Rebel Engine contributors
// SPDX-FileCopyrightText: 2014-2022 Godot Engine contributors
// SPDX-FileCopyrightText: 2007-2014 Juan Linietsky, Ariel Manzur
//
// SPDX-License-Identifier: MIT

#include "base_object_glue.h"

#ifdef MONO_GLUE_ENABLED

#include "../csharp_script.h"
#include "../mono_gd/gd_mono_cache.h"
#include "../mono_gd/gd_mono_class.h"
#include "../mono_gd/gd_mono_internals.h"
#include "../mono_gd/gd_mono_utils.h"
#include "../signal_awaiter_utils.h"
#include "arguments_vector.h"
#include "core/reference.h"
#include "core/string_name.h"

Object* rebel_icall_Object_Ctor(MonoObject* p_obj) {
    Object* instance = memnew(Object);
    GDMonoInternals::tie_managed_to_unmanaged(p_obj, instance);
    return instance;
}

void rebel_icall_Object_Disposed(MonoObject* p_obj, Object* p_ptr) {
#ifdef DEBUG_ENABLED
    CRASH_COND(p_ptr == NULL);
#endif

    if (p_ptr->get_script_instance()) {
        CSharpInstance* cs_instance =
            CAST_CSHARP_INSTANCE(p_ptr->get_script_instance());
        if (cs_instance) {
            if (!cs_instance->is_destructing_script_instance()) {
                cs_instance->mono_object_disposed(p_obj);
                p_ptr->set_script_instance(NULL);
            }
            return;
        }
    }

    void* data = p_ptr->get_script_instance_binding(
        CSharpLanguage::get_singleton()->get_language_index()
    );

    if (data) {
        CSharpScriptBinding& script_binding =
            ((Map<Object*, CSharpScriptBinding>::Element*)data)->get();
        if (script_binding.inited) {
            Ref<MonoGCHandle>& gchandle = script_binding.gchandle;
            if (gchandle.is_valid()) {
                CSharpLanguage::release_script_gchandle(p_obj, gchandle);
            }
        }
    }
}

void rebel_icall_Reference_Disposed(
    MonoObject* p_obj,
    Object* p_ptr,
    MonoBoolean p_is_finalizer
) {
#ifdef DEBUG_ENABLED
    CRASH_COND(p_ptr == NULL);
    // This is only called with Reference derived classes
    CRASH_COND(!Object::cast_to<Reference>(p_ptr));
#endif

    Reference* ref = static_cast<Reference*>(p_ptr);

    if (ref->get_script_instance()) {
        CSharpInstance* cs_instance =
            CAST_CSHARP_INSTANCE(ref->get_script_instance());
        if (cs_instance) {
            if (!cs_instance->is_destructing_script_instance()) {
                bool delete_owner;
                bool remove_script_instance;

                cs_instance->mono_object_disposed_baseref(
                    p_obj,
                    p_is_finalizer,
                    delete_owner,
                    remove_script_instance
                );

                if (delete_owner) {
                    memdelete(ref);
                } else if (remove_script_instance) {
                    ref->set_script_instance(NULL);
                }
            }
            return;
        }
    }

    // Unsafe refcount decrement. The managed instance also counts as a
    // reference. See: CSharpLanguage::alloc_instance_binding_data(Object
    // *p_object)
    CSharpLanguage::get_singleton()->pre_unsafe_unreference(ref);
    if (ref->unreference()) {
        memdelete(ref);
    } else {
        void* data = ref->get_script_instance_binding(
            CSharpLanguage::get_singleton()->get_language_index()
        );

        if (data) {
            CSharpScriptBinding& script_binding =
                ((Map<Object*, CSharpScriptBinding>::Element*)data)->get();
            if (script_binding.inited) {
                Ref<MonoGCHandle>& gchandle = script_binding.gchandle;
                if (gchandle.is_valid()) {
                    CSharpLanguage::release_script_gchandle(p_obj, gchandle);
                }
            }
        }
    }
}

MethodBind* rebel_icall_Object_ClassDB_get_method(
    MonoString* p_type,
    MonoString* p_method
) {
    StringName type(GDMonoMarshal::mono_string_to_rebel(p_type));
    StringName method(GDMonoMarshal::mono_string_to_rebel(p_method));
    return ClassDB::get_method(type, method);
}

MonoObject* rebel_icall_Object_weakref(Object* p_obj) {
    if (!p_obj) {
        return NULL;
    }

    Ref<WeakRef> wref;
    Reference* ref = Object::cast_to<Reference>(p_obj);

    if (ref) {
        REF r = ref;
        if (!r.is_valid()) {
            return NULL;
        }

        wref.instance();
        wref->set_ref(r);
    } else {
        wref.instance();
        wref->set_obj(p_obj);
    }

    return GDMonoUtils::unmanaged_get_managed(wref.ptr());
}

int32_t rebel_icall_SignalAwaiter_connect(
    Object* p_source,
    MonoString* p_signal,
    Object* p_target,
    MonoObject* p_awaiter
) {
    String signal = GDMonoMarshal::mono_string_to_rebel(p_signal);
    return (int32_t)SignalAwaiterUtils::connect_signal_awaiter(
        p_source,
        signal,
        p_target,
        p_awaiter
    );
}

MonoArray* rebel_icall_DynamicRebelObject_SetMemberList(Object* p_ptr) {
    List<PropertyInfo> property_list;
    p_ptr->get_property_list(&property_list);

    MonoArray* result = mono_array_new(
        mono_domain_get(),
        CACHED_CLASS_RAW(String),
        property_list.size()
    );

    int i = 0;
    for (List<PropertyInfo>::Element* E = property_list.front(); E;
         E                              = E->next()) {
        MonoString* boxed =
            GDMonoMarshal::mono_string_from_rebel(E->get().name);
        mono_array_setref(result, i, boxed);
        i++;
    }

    return result;
}

MonoBoolean rebel_icall_DynamicRebelObject_InvokeMember(
    Object* p_ptr,
    MonoString* p_name,
    MonoArray* p_args,
    MonoObject** r_result
) {
    String name = GDMonoMarshal::mono_string_to_rebel(p_name);

    int argc = mono_array_length(p_args);

    ArgumentsVector<Variant> arg_store(argc);
    ArgumentsVector<const Variant*> args(argc);

    for (int i = 0; i < argc; i++) {
        MonoObject* elem = mono_array_get(p_args, MonoObject*, i);
        arg_store.set(i, GDMonoMarshal::mono_object_to_variant(elem));
        args.set(i, &arg_store.get(i));
    }

    Variant::CallError error;
    Variant result = p_ptr->call(StringName(name), args.ptr(), argc, error);

    *r_result = GDMonoMarshal::variant_to_mono_object(result);

    return error.error == Variant::CallError::CALL_OK;
}

MonoBoolean rebel_icall_DynamicRebelObject_GetMember(
    Object* p_ptr,
    MonoString* p_name,
    MonoObject** r_result
) {
    String name = GDMonoMarshal::mono_string_to_rebel(p_name);

    bool valid;
    Variant value = p_ptr->get(StringName(name), &valid);

    if (valid) {
        *r_result = GDMonoMarshal::variant_to_mono_object(value);
    }

    return valid;
}

MonoBoolean rebel_icall_DynamicRebelObject_SetMember(
    Object* p_ptr,
    MonoString* p_name,
    MonoObject* p_value
) {
    String name   = GDMonoMarshal::mono_string_to_rebel(p_name);
    Variant value = GDMonoMarshal::mono_object_to_variant(p_value);

    bool valid;
    p_ptr->set(StringName(name), value, &valid);

    return valid;
}

MonoString* rebel_icall_Object_ToString(Object* p_ptr) {
#ifdef DEBUG_ENABLED
    // Cannot happen in C#; would get an ObjectDisposedException instead.
    CRASH_COND(p_ptr == NULL);

    if (ScriptDebugger::get_singleton()
        && !Object::cast_to<Reference>(p_ptr)) { // Only if debugging!
        // Cannot happen either in C#; the handle is nullified when the object
        // is destroyed
        CRASH_COND(!ObjectDB::instance_validate(p_ptr));
    }
#endif

    String result =
        "[" + p_ptr->get_class() + ":" + itos(p_ptr->get_instance_id()) + "]";
    return GDMonoMarshal::mono_string_from_rebel(result);
}

void rebel_register_object_icalls() {
    GDMonoUtils::add_internal_call(
        "Rebel.Object::rebel_icall_Object_Ctor",
        rebel_icall_Object_Ctor
    );
    GDMonoUtils::add_internal_call(
        "Rebel.Object::rebel_icall_Object_Disposed",
        rebel_icall_Object_Disposed
    );
    GDMonoUtils::add_internal_call(
        "Rebel.Object::rebel_icall_Reference_Disposed",
        rebel_icall_Reference_Disposed
    );
    GDMonoUtils::add_internal_call(
        "Rebel.Object::rebel_icall_Object_ClassDB_get_method",
        rebel_icall_Object_ClassDB_get_method
    );
    GDMonoUtils::add_internal_call(
        "Rebel.Object::rebel_icall_Object_ToString",
        rebel_icall_Object_ToString
    );
    GDMonoUtils::add_internal_call(
        "Rebel.Object::rebel_icall_Object_weakref",
        rebel_icall_Object_weakref
    );
    GDMonoUtils::add_internal_call(
        "Rebel.SignalAwaiter::rebel_icall_SignalAwaiter_connect",
        rebel_icall_SignalAwaiter_connect
    );
    GDMonoUtils::add_internal_call(
        "Rebel.DynamicRebelObject::rebel_icall_DynamicRebelObject_"
        "SetMemberList",
        rebel_icall_DynamicRebelObject_SetMemberList
    );
    GDMonoUtils::add_internal_call(
        "Rebel.DynamicRebelObject::rebel_icall_DynamicRebelObject_InvokeMember",
        rebel_icall_DynamicRebelObject_InvokeMember
    );
    GDMonoUtils::add_internal_call(
        "Rebel.DynamicRebelObject::rebel_icall_DynamicRebelObject_GetMember",
        rebel_icall_DynamicRebelObject_GetMember
    );
    GDMonoUtils::add_internal_call(
        "Rebel.DynamicRebelObject::rebel_icall_DynamicRebelObject_SetMember",
        rebel_icall_DynamicRebelObject_SetMember
    );
}

#endif // MONO_GLUE_ENABLED
