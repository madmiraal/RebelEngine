// SPDX-FileCopyrightText: 2023 Rebel Engine contributors
// SPDX-FileCopyrightText: 2014-2022 Godot Engine contributors
// SPDX-FileCopyrightText: 2007-2014 Juan Linietsky, Ariel Manzur
//
// SPDX-License-Identifier: MIT

#include "gd_glue.h"

#ifdef MONO_GLUE_ENABLED

#include "../mono_gd/gd_mono_cache.h"
#include "../mono_gd/gd_mono_utils.h"
#include "core/array.h"
#include "core/io/marshalls.h"
#include "core/os/os.h"
#include "core/ustring.h"
#include "core/variant.h"
#include "core/variant_parser.h"

MonoObject* rebel_icall_GD_bytes2var(
    MonoArray* p_bytes,
    MonoBoolean p_allow_objects
) {
    Variant ret;
    PoolByteArray varr    = GDMonoMarshal::mono_array_to_PoolByteArray(p_bytes);
    PoolByteArray::Read r = varr.read();
    Error err =
        decode_variant(ret, r.ptr(), varr.size(), NULL, p_allow_objects);
    if (err != OK) {
        ret = RTR("Not enough bytes for decoding bytes, or invalid format.");
    }
    return GDMonoMarshal::variant_to_mono_object(ret);
}

MonoObject* rebel_icall_GD_convert(MonoObject* p_what, int32_t p_type) {
    Variant what           = GDMonoMarshal::mono_object_to_variant(p_what);
    const Variant* args[1] = {&what};
    Variant::CallError ce;
    Variant ret = Variant::construct(Variant::Type(p_type), args, 1, ce);
    ERR_FAIL_COND_V(ce.error != Variant::CallError::CALL_OK, NULL);
    return GDMonoMarshal::variant_to_mono_object(ret);
}

int rebel_icall_GD_hash(MonoObject* p_var) {
    return GDMonoMarshal::mono_object_to_variant(p_var).hash();
}

MonoObject* rebel_icall_GD_instance_from_id(uint64_t p_instance_id) {
    return GDMonoUtils::unmanaged_get_managed(
        ObjectDB::get_instance(p_instance_id)
    );
}

void rebel_icall_GD_print(MonoArray* p_what) {
    String str;
    int length = mono_array_length(p_what);

    for (int i = 0; i < length; i++) {
        MonoObject* elem = mono_array_get(p_what, MonoObject*, i);

        MonoException* exc = NULL;
        String elem_str =
            GDMonoMarshal::mono_object_to_variant_string(elem, &exc);

        if (exc) {
            GDMonoUtils::set_pending_exception(exc);
            return;
        }

        str += elem_str;
    }

    print_line(str);
}

void rebel_icall_GD_printerr(MonoArray* p_what) {
    String str;
    int length = mono_array_length(p_what);

    for (int i = 0; i < length; i++) {
        MonoObject* elem = mono_array_get(p_what, MonoObject*, i);

        MonoException* exc = NULL;
        String elem_str =
            GDMonoMarshal::mono_object_to_variant_string(elem, &exc);

        if (exc) {
            GDMonoUtils::set_pending_exception(exc);
            return;
        }

        str += elem_str;
    }

    print_error(str);
}

void rebel_icall_GD_printraw(MonoArray* p_what) {
    String str;
    int length = mono_array_length(p_what);

    for (int i = 0; i < length; i++) {
        MonoObject* elem = mono_array_get(p_what, MonoObject*, i);

        MonoException* exc = NULL;
        String elem_str =
            GDMonoMarshal::mono_object_to_variant_string(elem, &exc);

        if (exc) {
            GDMonoUtils::set_pending_exception(exc);
            return;
        }

        str += elem_str;
    }

    OS::get_singleton()->print("%s", str.utf8().get_data());
}

void rebel_icall_GD_prints(MonoArray* p_what) {
    String str;
    int length = mono_array_length(p_what);

    for (int i = 0; i < length; i++) {
        MonoObject* elem = mono_array_get(p_what, MonoObject*, i);

        MonoException* exc = NULL;
        String elem_str =
            GDMonoMarshal::mono_object_to_variant_string(elem, &exc);

        if (exc) {
            GDMonoUtils::set_pending_exception(exc);
            return;
        }

        if (i) {
            str += " ";
        }

        str += elem_str;
    }

    print_line(str);
}

void rebel_icall_GD_printt(MonoArray* p_what) {
    String str;
    int length = mono_array_length(p_what);

    for (int i = 0; i < length; i++) {
        MonoObject* elem = mono_array_get(p_what, MonoObject*, i);

        MonoException* exc = NULL;
        String elem_str =
            GDMonoMarshal::mono_object_to_variant_string(elem, &exc);

        if (exc) {
            GDMonoUtils::set_pending_exception(exc);
            return;
        }

        if (i) {
            str += "\t";
        }

        str += elem_str;
    }

    print_line(str);
}

float rebel_icall_GD_randf() {
    return Math::randf();
}

uint32_t rebel_icall_GD_randi() {
    return Math::rand();
}

void rebel_icall_GD_randomize() {
    Math::randomize();
}

double rebel_icall_GD_rand_range(double from, double to) {
    return Math::random(from, to);
}

uint32_t rebel_icall_GD_rand_seed(uint64_t seed, uint64_t* newSeed) {
    int ret  = Math::rand_from_seed(&seed);
    *newSeed = seed;
    return ret;
}

void rebel_icall_GD_seed(uint64_t p_seed) {
    Math::seed(p_seed);
}

MonoString* rebel_icall_GD_str(MonoArray* p_what) {
    String str;
    Array what = GDMonoMarshal::mono_array_to_Array(p_what);

    for (int i = 0; i < what.size(); i++) {
        String os = what[i].operator String();

        if (i == 0) {
            str = os;
        } else {
            str += os;
        }
    }

    return GDMonoMarshal::mono_string_from_rebel(str);
}

MonoObject* rebel_icall_GD_str2var(MonoString* p_str) {
    Variant ret;

    VariantParser::StreamString ss;
    ss.s = GDMonoMarshal::mono_string_to_rebel(p_str);

    String errs;
    int line;
    Error err = VariantParser::parse(&ss, ret, errs, line);
    if (err != OK) {
        String err_str =
            "Parse error at line " + itos(line) + ": " + errs + ".";
        ERR_PRINT(err_str);
        ret = err_str;
    }

    return GDMonoMarshal::variant_to_mono_object(ret);
}

MonoBoolean rebel_icall_GD_type_exists(MonoString* p_type) {
    return ClassDB::class_exists(GDMonoMarshal::mono_string_to_rebel(p_type));
}

void rebel_icall_GD_pusherror(MonoString* p_str) {
    ERR_PRINT(GDMonoMarshal::mono_string_to_rebel(p_str));
}

void rebel_icall_GD_pushwarning(MonoString* p_str) {
    WARN_PRINT(GDMonoMarshal::mono_string_to_rebel(p_str));
}

MonoArray* rebel_icall_GD_var2bytes(
    MonoObject* p_var,
    MonoBoolean p_full_objects
) {
    Variant var = GDMonoMarshal::mono_object_to_variant(p_var);

    PoolByteArray barr;
    int len;
    Error err = encode_variant(var, NULL, len, p_full_objects);
    ERR_FAIL_COND_V_MSG(
        err != OK,
        NULL,
        "Unexpected error encoding variable to bytes, likely unserializable "
        "type found (Object or RID)."
    );

    barr.resize(len);
    {
        PoolByteArray::Write w = barr.write();
        encode_variant(var, w.ptr(), len, p_full_objects);
    }

    return GDMonoMarshal::PoolByteArray_to_mono_array(barr);
}

MonoString* rebel_icall_GD_var2str(MonoObject* p_var) {
    String vars;
    VariantWriter::write_to_string(
        GDMonoMarshal::mono_object_to_variant(p_var),
        vars
    );
    return GDMonoMarshal::mono_string_from_rebel(vars);
}

MonoObject* rebel_icall_DefaultRebelTaskScheduler() {
    return GDMonoCache::cached_data.task_scheduler_handle->get_target();
}

void rebel_register_gd_icalls() {
    GDMonoUtils::add_internal_call(
        "Rebel.GD::rebel_icall_GD_bytes2var",
        rebel_icall_GD_bytes2var
    );
    GDMonoUtils::add_internal_call(
        "Rebel.GD::rebel_icall_GD_convert",
        rebel_icall_GD_convert
    );
    GDMonoUtils::add_internal_call(
        "Rebel.GD::rebel_icall_GD_hash",
        rebel_icall_GD_hash
    );
    GDMonoUtils::add_internal_call(
        "Rebel.GD::rebel_icall_GD_instance_from_id",
        rebel_icall_GD_instance_from_id
    );
    GDMonoUtils::add_internal_call(
        "Rebel.GD::rebel_icall_GD_pusherror",
        rebel_icall_GD_pusherror
    );
    GDMonoUtils::add_internal_call(
        "Rebel.GD::rebel_icall_GD_pushwarning",
        rebel_icall_GD_pushwarning
    );
    GDMonoUtils::add_internal_call(
        "Rebel.GD::rebel_icall_GD_print",
        rebel_icall_GD_print
    );
    GDMonoUtils::add_internal_call(
        "Rebel.GD::rebel_icall_GD_printerr",
        rebel_icall_GD_printerr
    );
    GDMonoUtils::add_internal_call(
        "Rebel.GD::rebel_icall_GD_printraw",
        rebel_icall_GD_printraw
    );
    GDMonoUtils::add_internal_call(
        "Rebel.GD::rebel_icall_GD_prints",
        rebel_icall_GD_prints
    );
    GDMonoUtils::add_internal_call(
        "Rebel.GD::rebel_icall_GD_printt",
        rebel_icall_GD_printt
    );
    GDMonoUtils::add_internal_call(
        "Rebel.GD::rebel_icall_GD_randf",
        rebel_icall_GD_randf
    );
    GDMonoUtils::add_internal_call(
        "Rebel.GD::rebel_icall_GD_randi",
        rebel_icall_GD_randi
    );
    GDMonoUtils::add_internal_call(
        "Rebel.GD::rebel_icall_GD_randomize",
        rebel_icall_GD_randomize
    );
    GDMonoUtils::add_internal_call(
        "Rebel.GD::rebel_icall_GD_rand_range",
        rebel_icall_GD_rand_range
    );
    GDMonoUtils::add_internal_call(
        "Rebel.GD::rebel_icall_GD_rand_seed",
        rebel_icall_GD_rand_seed
    );
    GDMonoUtils::add_internal_call(
        "Rebel.GD::rebel_icall_GD_seed",
        rebel_icall_GD_seed
    );
    GDMonoUtils::add_internal_call(
        "Rebel.GD::rebel_icall_GD_str",
        rebel_icall_GD_str
    );
    GDMonoUtils::add_internal_call(
        "Rebel.GD::rebel_icall_GD_str2var",
        rebel_icall_GD_str2var
    );
    GDMonoUtils::add_internal_call(
        "Rebel.GD::rebel_icall_GD_type_exists",
        rebel_icall_GD_type_exists
    );
    GDMonoUtils::add_internal_call(
        "Rebel.GD::rebel_icall_GD_var2bytes",
        rebel_icall_GD_var2bytes
    );
    GDMonoUtils::add_internal_call(
        "Rebel.GD::rebel_icall_GD_var2str",
        rebel_icall_GD_var2str
    );

    // Dispatcher
    GDMonoUtils::add_internal_call(
        "Rebel.Dispatcher::rebel_icall_DefaultRebelTaskScheduler",
        rebel_icall_DefaultRebelTaskScheduler
    );
}

#endif // MONO_GLUE_ENABLED
