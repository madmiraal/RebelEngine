// SPDX-FileCopyrightText: 2023 Rebel Engine contributors
// SPDX-FileCopyrightText: 2014-2022 Godot Engine contributors
// SPDX-FileCopyrightText: 2007-2014 Juan Linietsky, Ariel Manzur
//
// SPDX-License-Identifier: MIT

#include "rebelsharp_dirs.h"

#include "core/os/dir_access.h"
#include "core/os/os.h"
#include "core/project_settings.h"

#ifdef TOOLS_ENABLED
#include "core/version.h"
#include "editor/editor_settings.h"
#endif

#ifdef ANDROID_ENABLED
#include "mono_gd/support/android_support.h"
#endif

#include "mono_gd/gd_mono.h"

namespace RebelSharpDirs {

String _get_expected_build_config() {
#ifdef TOOLS_ENABLED
    return "Debug";
#else

#ifdef DEBUG_ENABLED
    return "ExportDebug";
#else
    return "ExportRelease";
#endif

#endif
}

String _get_mono_user_dir() {
#ifdef TOOLS_ENABLED
    if (EditorSettings::get_singleton()) {
        return EditorSettings::get_singleton()->get_data_dir().plus_file("mono"
        );
    } else {
        String settings_path;

        String exe_dir =
            OS::get_singleton()->get_executable_path().get_base_dir();
        DirAccessRef d = DirAccess::create_for_path(exe_dir);

        if (d->file_exists("._sc_") || d->file_exists("_sc_")) {
            // contain yourself
            settings_path = exe_dir.plus_file("editor_data");
        } else {
            settings_path = OS::get_singleton()->get_data_path().plus_file(
                OS::get_singleton()->get_rebel_dir_name()
            );
        }

        return settings_path.plus_file("mono");
    }
#else
    return OS::get_singleton()->get_user_data_dir().plus_file("mono");
#endif
}

class _RebelSharpDirs {
public:
    String res_data_dir;
    String res_metadata_dir;
    String res_assemblies_base_dir;
    String res_assemblies_dir;
    String res_config_dir;
    String res_temp_dir;
    String res_temp_assemblies_base_dir;
    String res_temp_assemblies_dir;
    String mono_user_dir;
    String mono_logs_dir;

#ifdef TOOLS_ENABLED
    String mono_solutions_dir;
    String build_logs_dir;

    String sln_filepath;
    String csproj_filepath;

    String data_editor_tools_dir;
    String data_editor_prebuilt_api_dir;
#else
    // Equivalent of res_assemblies_dir, but in the data directory rather than
    // in 'res://'. Only defined on export templates. Used when exporting
    // assemblies outside of PCKs.
    String data_game_assemblies_dir;
#endif

    String data_mono_etc_dir;
    String data_mono_lib_dir;

#ifdef WINDOWS_ENABLED
    String data_mono_bin_dir;
#endif

private:
    _RebelSharpDirs() {
        res_data_dir            = "res://.mono";
        res_metadata_dir        = res_data_dir.plus_file("metadata");
        res_assemblies_base_dir = res_data_dir.plus_file("assemblies");
        res_assemblies_dir      = res_assemblies_base_dir.plus_file(
            GDMono::get_expected_api_build_config()
        );
        res_config_dir = res_data_dir.plus_file("etc").plus_file("mono");

        // TODO use paths from csproj
        res_temp_dir                 = res_data_dir.plus_file("temp");
        res_temp_assemblies_base_dir = res_temp_dir.plus_file("bin");
        res_temp_assemblies_dir =
            res_temp_assemblies_base_dir.plus_file(_get_expected_build_config()
            );

#ifdef WEB_ENABLED
        mono_user_dir = "user://";
#else
        mono_user_dir = _get_mono_user_dir();
#endif
        mono_logs_dir = mono_user_dir.plus_file("mono_logs");

#ifdef TOOLS_ENABLED
        mono_solutions_dir = mono_user_dir.plus_file("solutions");
        build_logs_dir     = mono_user_dir.plus_file("build_logs");

        String appname =
            ProjectSettings::get_singleton()->get("application/config/name");
        String appname_safe = OS::get_singleton()->get_safe_dir_name(appname);
        if (appname_safe.empty()) {
            appname_safe = "UnnamedProject";
        }

        String base_path =
            ProjectSettings::get_singleton()->globalize_path("res://");

        sln_filepath    = base_path.plus_file(appname_safe + ".sln");
        csproj_filepath = base_path.plus_file(appname_safe + ".csproj");
#endif

        String exe_dir =
            OS::get_singleton()->get_executable_path().get_base_dir();

#ifdef TOOLS_ENABLED

        String data_dir_root         = exe_dir.plus_file("RebelSharp");
        data_editor_tools_dir        = data_dir_root.plus_file("Tools");
        data_editor_prebuilt_api_dir = data_dir_root.plus_file("Api");

        String data_mono_root_dir = data_dir_root.plus_file("Mono");
        data_mono_etc_dir         = data_mono_root_dir.plus_file("etc");

#ifdef ANDROID_ENABLED
        data_mono_lib_dir = gdmono::android::support::get_app_native_lib_dir();
#else
        data_mono_lib_dir = data_mono_root_dir.plus_file("lib");
#endif

#ifdef WINDOWS_ENABLED
        data_mono_bin_dir = data_mono_root_dir.plus_file("bin");
#endif

#ifdef MACOS_ENABLED
        if (!DirAccess::exists(data_editor_tools_dir)) {
            data_editor_tools_dir =
                exe_dir.plus_file("../Resources/RebelSharp/Tools");
        }

        if (!DirAccess::exists(data_editor_prebuilt_api_dir)) {
            data_editor_prebuilt_api_dir =
                exe_dir.plus_file("../Resources/RebelSharp/Api");
        }

        if (!DirAccess::exists(data_mono_root_dir)) {
            data_mono_etc_dir =
                exe_dir.plus_file("../Resources/RebelSharp/Mono/etc");
            data_mono_lib_dir =
                exe_dir.plus_file("../Resources/RebelSharp/Mono/lib");
        }
#endif

#else

        String appname =
            ProjectSettings::get_singleton()->get("application/config/name");
        String appname_safe  = OS::get_singleton()->get_safe_dir_name(appname);
        String data_dir_root = exe_dir.plus_file("data_" + appname_safe);
        if (!DirAccess::exists(data_dir_root)) {
            data_dir_root = exe_dir.plus_file("data_Rebel");
        }

        String data_mono_root_dir = data_dir_root.plus_file("Mono");
        data_mono_etc_dir         = data_mono_root_dir.plus_file("etc");

#ifdef ANDROID_ENABLED
        data_mono_lib_dir = gdmono::android::support::get_app_native_lib_dir();
#else
        data_mono_lib_dir        = data_mono_root_dir.plus_file("lib");
        data_game_assemblies_dir = data_dir_root.plus_file("Assemblies");
#endif

#ifdef WINDOWS_ENABLED
        data_mono_bin_dir = data_mono_root_dir.plus_file("bin");
#endif

#ifdef MACOS_ENABLED
        if (!DirAccess::exists(data_mono_root_dir)) {
            data_mono_etc_dir =
                exe_dir.plus_file("../Resources/RebelSharp/Mono/etc");
            data_mono_lib_dir =
                exe_dir.plus_file("../Resources/RebelSharp/Mono/lib");
        }

        if (!DirAccess::exists(data_game_assemblies_dir)) {
            data_game_assemblies_dir =
                exe_dir.plus_file("../Resources/RebelSharp/Assemblies");
        }
#endif

#endif
    }

    _RebelSharpDirs(const _RebelSharpDirs&);
    _RebelSharpDirs& operator=(const _RebelSharpDirs&);

public:
    static _RebelSharpDirs& get_singleton() {
        static _RebelSharpDirs singleton;
        return singleton;
    }
};

String get_res_data_dir() {
    return _RebelSharpDirs::get_singleton().res_data_dir;
}

String get_res_metadata_dir() {
    return _RebelSharpDirs::get_singleton().res_metadata_dir;
}

String get_res_assemblies_base_dir() {
    return _RebelSharpDirs::get_singleton().res_assemblies_base_dir;
}

String get_res_assemblies_dir() {
    return _RebelSharpDirs::get_singleton().res_assemblies_dir;
}

String get_res_config_dir() {
    return _RebelSharpDirs::get_singleton().res_config_dir;
}

String get_res_temp_dir() {
    return _RebelSharpDirs::get_singleton().res_temp_dir;
}

String get_res_temp_assemblies_base_dir() {
    return _RebelSharpDirs::get_singleton().res_temp_assemblies_base_dir;
}

String get_res_temp_assemblies_dir() {
    return _RebelSharpDirs::get_singleton().res_temp_assemblies_dir;
}

String get_mono_user_dir() {
    return _RebelSharpDirs::get_singleton().mono_user_dir;
}

String get_mono_logs_dir() {
    return _RebelSharpDirs::get_singleton().mono_logs_dir;
}

#ifdef TOOLS_ENABLED
String get_mono_solutions_dir() {
    return _RebelSharpDirs::get_singleton().mono_solutions_dir;
}

String get_build_logs_dir() {
    return _RebelSharpDirs::get_singleton().build_logs_dir;
}

String get_project_sln_path() {
    return _RebelSharpDirs::get_singleton().sln_filepath;
}

String get_project_csproj_path() {
    return _RebelSharpDirs::get_singleton().csproj_filepath;
}

String get_data_editor_tools_dir() {
    return _RebelSharpDirs::get_singleton().data_editor_tools_dir;
}

String get_data_editor_prebuilt_api_dir() {
    return _RebelSharpDirs::get_singleton().data_editor_prebuilt_api_dir;
}
#else
String get_data_game_assemblies_dir() {
    return _RebelSharpDirs::get_singleton().data_game_assemblies_dir;
}
#endif

String get_data_mono_etc_dir() {
    return _RebelSharpDirs::get_singleton().data_mono_etc_dir;
}

String get_data_mono_lib_dir() {
    return _RebelSharpDirs::get_singleton().data_mono_lib_dir;
}

#ifdef WINDOWS_ENABLED
String get_data_mono_bin_dir() {
    return _RebelSharpDirs::get_singleton().data_mono_bin_dir;
}
#endif

} // namespace RebelSharpDirs
