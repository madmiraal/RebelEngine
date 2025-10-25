"""Functions used to generate source files during build time

All such functions are invoked in a subprocess on Windows to prevent build flakiness.

"""
import json
from platform_methods import subprocess_main


def _spaced(e):
    return e if e[-1] == "*" else e + " "


def _build_gdnative_api_struct_header(api):
    gdnative_api_init_macro = [
        "    extern const rebel_gdnative_core_api_struct* _gdnative_wrapper_api_struct;"
    ]

    for ext in api["extensions"]:
        name = ext["name"]
        gdnative_api_init_macro.append(
            "    extern const rebel_gdnative_ext_{0}_api_struct* _gdnative_wrapper_{0}_api_struct;".format(
                name
            )
        )

    gdnative_api_init_macro.append(
        "    _gdnative_wrapper_api_struct = options->api_struct;"
    )
    gdnative_api_init_macro.append(
        "    for (unsigned int i = 0; i < _gdnative_wrapper_api_struct->num_extensions; i++) { "
    )
    gdnative_api_init_macro.append(
        "        switch (_gdnative_wrapper_api_struct->extensions[i]->type) {"
    )

    for ext in api["extensions"]:
        name = ext["name"]
        gdnative_api_init_macro.append(
            "            case GDNATIVE_EXT_%s:" % ext["type"]
        )
        gdnative_api_init_macro.append(
            "                _gdnative_wrapper_{0}_api_struct = (rebel_gdnative_ext_{0}_api_struct*)"
            " _gdnative_wrapper_api_struct->extensions[i];".format(name)
        )
        gdnative_api_init_macro.append("                break;")
    gdnative_api_init_macro.append("        }")
    gdnative_api_init_macro.append("    }")

    out = [
        "/* THIS FILE IS GENERATED DO NOT EDIT */",
        "#ifndef GDNATIVE_API_STRUCT_H",
        "#define GDNATIVE_API_STRUCT_H",
        "",
        "#include <gdnative/gdnative.h>",
        "#include <android/rebel_android.h>",
        "#include <arvr/rebel_arvr.h>",
        "#include <nativescript/rebel_nativescript.h>",
        "#include <net/rebel_net.h>",
        "#include <pluginscript/rebel_pluginscript.h>",
        "#include <videodecoder/rebel_videodecoder.h>",
        "",
        "#define GDNATIVE_API_INIT(options) do {  \\\n"
        + "  \\\n".join(gdnative_api_init_macro)
        + "  \\\n } while (0)",
        "",
        "#ifdef __cplusplus",
        'extern "C" {',
        "#endif",
        "",
        "enum GDNATIVE_API_TYPES {",
        "    GDNATIVE_" + api["core"]["type"] + ",",
    ]

    for ext in api["extensions"]:
        out += ["    GDNATIVE_EXT_" + ext["type"] + ","]

    out += ["};", ""]

    def generate_extension_struct(name, ext, include_version=True):
        ret_val = []
        if ext["next"]:
            ret_val += generate_extension_struct(name, ext["next"])

        ret_val += [
            "typedef struct rebel_gdnative_ext_"
            + name
            + (
                ""
                if not include_version
                else (
                    "_{0}_{1}".format(ext["version"]["major"], ext["version"]["minor"])
                )
            )
            + "_api_struct {",
            "    unsigned int type;",
            "    rebel_gdnative_api_version version;",
            "    const rebel_gdnative_api_struct* next;",
        ]

        for funcdef in ext["api"]:
            args = ", ".join(
                ["%s%s" % (_spaced(t), n) for t, n in funcdef["arguments"]]
            )
            ret_val.append(
                "    %s(*%s)(%s);"
                % (_spaced(funcdef["return_type"]), funcdef["name"], args)
            )

        ret_val += [
            "} rebel_gdnative_ext_"
            + name
            + (
                ""
                if not include_version
                else (
                    "_{0}_{1}".format(ext["version"]["major"], ext["version"]["minor"])
                )
            )
            + "_api_struct;",
            "",
        ]

        return ret_val

    def generate_core_extension_struct(core):
        ret_val = []
        if core["next"]:
            ret_val += generate_core_extension_struct(core["next"])

        ret_val += [
            "typedef struct rebel_gdnative_core_"
            + ("{0}_{1}".format(core["version"]["major"], core["version"]["minor"]))
            + "_api_struct {",
            "    unsigned int type;",
            "    rebel_gdnative_api_version version;",
            "    const rebel_gdnative_api_struct* next;",
        ]

        for funcdef in core["api"]:
            args = ", ".join(
                ["%s%s" % (_spaced(t), n) for t, n in funcdef["arguments"]]
            )
            ret_val.append(
                "    %s(*%s)(%s);"
                % (_spaced(funcdef["return_type"]), funcdef["name"], args)
            )

        ret_val += [
            "} rebel_gdnative_core_"
            + "{0}_{1}".format(core["version"]["major"], core["version"]["minor"])
            + "_api_struct;",
            "",
        ]

        return ret_val

    for ext in api["extensions"]:
        name = ext["name"]
        out += generate_extension_struct(name, ext, False)

    if api["core"]["next"]:
        out += generate_core_extension_struct(api["core"]["next"])

    out += [
        "typedef struct rebel_gdnative_core_api_struct {",
        "    unsigned int type;",
        "    rebel_gdnative_api_version version;",
        "    const rebel_gdnative_api_struct* next;",
        "    unsigned int num_extensions;",
        "    const rebel_gdnative_api_struct** extensions;",
    ]

    for funcdef in api["core"]["api"]:
        args = ", ".join(["%s%s" % (_spaced(t), n) for t, n in funcdef["arguments"]])
        out.append(
            "    %s(*%s)(%s);"
            % (_spaced(funcdef["return_type"]), funcdef["name"], args)
        )

    out += [
        "} rebel_gdnative_core_api_struct;",
        "",
        "#ifdef __cplusplus",
        "}",
        "#endif",
        "",
        "#endif // GDNATIVE_API_STRUCT_H",
        "",
    ]
    return "\n".join(out)


def _build_gdnative_api_struct_source(api):
    out = [
        "/* THIS FILE IS GENERATED DO NOT EDIT */",
        "",
        "#include <gdnative_api_struct.gen.h>",
        "",
    ]

    def get_extension_struct_name(name, ext, include_version=True):
        return (
            "rebel_gdnative_ext_"
            + name
            + (
                ""
                if not include_version
                else (
                    "_{0}_{1}".format(ext["version"]["major"], ext["version"]["minor"])
                )
            )
            + "_api_struct"
        )

    def get_extension_struct_instance_name(name, ext, include_version=True):
        return (
            "api_extension_"
            + name
            + (
                ""
                if not include_version
                else (
                    "_{0}_{1}".format(ext["version"]["major"], ext["version"]["minor"])
                )
            )
            + "_struct"
        )

    def get_extension_struct_definition(name, ext, include_version=True):
        ret_val = []

        if ext["next"]:
            ret_val += get_extension_struct_definition(name, ext["next"])

        ret_val += [
            "extern const "
            + get_extension_struct_name(name, ext, include_version)
            + " "
            + get_extension_struct_instance_name(name, ext, include_version)
            + " = {",
            "    GDNATIVE_EXT_" + ext["type"] + ",",
            "    {"
            + str(ext["version"]["major"])
            + ", "
            + str(ext["version"]["minor"])
            + "},",
            "    "
            + (
                "NULL"
                if not ext["next"]
                else (
                    "(const rebel_gdnative_api_struct* )&"
                    + get_extension_struct_instance_name(name, ext["next"])
                )
            )
            + ",",
        ]

        for funcdef in ext["api"]:
            ret_val.append("    %s," % funcdef["name"])

        ret_val += ["};\n"]

        return ret_val

    def get_core_struct_definition(core):
        ret_val = []

        if core["next"]:
            ret_val += get_core_struct_definition(core["next"])

        ret_val += [
            "extern const rebel_gdnative_core_"
            + (
                "{0}_{1}_api_struct api_{0}_{1}".format(
                    core["version"]["major"], core["version"]["minor"]
                )
            )
            + " = {",
            "    GDNATIVE_" + core["type"] + ",",
            "    {"
            + str(core["version"]["major"])
            + ", "
            + str(core["version"]["minor"])
            + "},",
            "    "
            + (
                "NULL"
                if not core["next"]
                else (
                    "(const rebel_gdnative_api_struct* )& api_{0}_{1}".format(
                        core["next"]["version"]["major"],
                        core["next"]["version"]["minor"],
                    )
                )
            )
            + ",",
        ]

        for funcdef in core["api"]:
            ret_val.append("    %s," % funcdef["name"])

        ret_val += ["};\n"]

        return ret_val

    for ext in api["extensions"]:
        name = ext["name"]
        out += get_extension_struct_definition(name, ext, False)

    out += ["", "const rebel_gdnative_api_struct* gdnative_extensions_pointers[] = {"]

    for ext in api["extensions"]:
        name = ext["name"]
        out += ["    (rebel_gdnative_api_struct*)&api_extension_" + name + "_struct,"]

    out += ["};\n"]

    if api["core"]["next"]:
        out += get_core_struct_definition(api["core"]["next"])

    out += [
        "extern const rebel_gdnative_core_api_struct api_struct = {",
        "    GDNATIVE_" + api["core"]["type"] + ",",
        "    {"
        + str(api["core"]["version"]["major"])
        + ", "
        + str(api["core"]["version"]["minor"])
        + "},",
        "    (const rebel_gdnative_api_struct*)&api_1_1,",
        "    " + str(len(api["extensions"])) + ",",
        "    gdnative_extensions_pointers,",
    ]

    for funcdef in api["core"]["api"]:
        out.append("    %s," % funcdef["name"])
    out.append("};\n")

    return "\n".join(out)


def build_gdnative_api_struct(target, source, env):
    with open(source[0], "r") as fd:
        api = json.load(fd)

    header, source = target
    with open(header, "w") as fd:
        fd.write(_build_gdnative_api_struct_header(api))
    with open(source, "w") as fd:
        fd.write(_build_gdnative_api_struct_source(api))


def _build_gdnative_wrapper_code(api):
    out = [
        "/* THIS FILE IS GENERATED DO NOT EDIT */",
        "",
        "#include <gdnative/gdnative.h>",
        "#include <nativescript/rebel_nativescript.h>",
        "#include <pluginscript/rebel_pluginscript.h>",
        "#include <arvr/rebel_arvr.h>",
        "#include <videodecoder/rebel_videodecoder.h>",
        "",
        "#include <gdnative_api_struct.gen.h>",
        "",
        "#ifdef __cplusplus",
        'extern "C" {',
        "#endif",
        "",
        "rebel_gdnative_core_api_struct* _gdnative_wrapper_api_struct = 0;",
    ]

    for ext in api["extensions"]:
        name = ext["name"]
        out.append(
            "rebel_gdnative_ext_"
            + name
            + "_api_struct* _gdnative_wrapper_"
            + name
            + "_api_struct = 0;"
        )

    out += [""]

    for funcdef in api["core"]["api"]:
        args = ", ".join(["%s%s" % (_spaced(t), n) for t, n in funcdef["arguments"]])
        out.append(
            "%s%s(%s) {" % (_spaced(funcdef["return_type"]), funcdef["name"], args)
        )

        args = ", ".join(["%s" % n for t, n in funcdef["arguments"]])

        return_line = "    return " if funcdef["return_type"] != "void" else "    "
        return_line += (
            "_gdnative_wrapper_api_struct->" + funcdef["name"] + "(" + args + ");"
        )

        out.append(return_line)
        out.append("}")
        out.append("")

    for ext in api["extensions"]:
        name = ext["name"]
        for funcdef in ext["api"]:
            args = ", ".join(
                ["%s%s" % (_spaced(t), n) for t, n in funcdef["arguments"]]
            )
            out.append(
                "%s%s(%s) {" % (_spaced(funcdef["return_type"]), funcdef["name"], args)
            )

            args = ", ".join(["%s" % n for t, n in funcdef["arguments"]])

            return_line = "    return " if funcdef["return_type"] != "void" else "    "
            return_line += (
                "_gdnative_wrapper_"
                + name
                + "_api_struct->"
                + funcdef["name"]
                + "("
                + args
                + ");"
            )

            out.append(return_line)
            out.append("}")
            out.append("")

    out += ["#ifdef __cplusplus", "}", "#endif"]

    return "\n".join(out)


def build_gdnative_wrapper_code(target, source, env):
    with open(source[0], "r") as fd:
        api = json.load(fd)

    wrapper_file = target[0]
    with open(wrapper_file, "w") as fd:
        fd.write(_build_gdnative_wrapper_code(api))


if __name__ == "__main__":
    subprocess_main(globals())
