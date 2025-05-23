// SPDX-FileCopyrightText: 2023 Rebel Engine contributors
// SPDX-FileCopyrightText: 2014-2022 Godot Engine contributors
// SPDX-FileCopyrightText: 2007-2014 Juan Linietsky, Ariel Manzur
//
// SPDX-License-Identifier: MIT

#include "editor_internal_calls.h"

#ifdef UNIX_ENABLED
#include <unistd.h> // access
#endif

#include "../csharp_script.h"
#include "../glue/cs_glue_version.gen.h"
#include "../mono_gd/gd_mono_marshal.h"
#include "../rebelsharp_dirs.h"
#include "../utils/macos_utils.h"
#include "code_completion.h"
#include "core/os/os.h"
#include "core/version.h"
#include "editor/editor_node.h"
#include "editor/editor_scale.h"
#include "editor/plugins/script_editor_plugin.h"
#include "editor/script_editor_debugger.h"
#include "main/main.h"
#include "rebelsharp_export.h"
#include "script_class_parser.h"

MonoString* rebel_icall_RebelSharpDirs_ResDataDir() {
    return GDMonoMarshal::mono_string_from_rebel(
        RebelSharpDirs::get_res_data_dir()
    );
}

MonoString* rebel_icall_RebelSharpDirs_ResMetadataDir() {
    return GDMonoMarshal::mono_string_from_rebel(
        RebelSharpDirs::get_res_metadata_dir()
    );
}

MonoString* rebel_icall_RebelSharpDirs_ResAssembliesBaseDir() {
    return GDMonoMarshal::mono_string_from_rebel(
        RebelSharpDirs::get_res_assemblies_base_dir()
    );
}

MonoString* rebel_icall_RebelSharpDirs_ResAssembliesDir() {
    return GDMonoMarshal::mono_string_from_rebel(
        RebelSharpDirs::get_res_assemblies_dir()
    );
}

MonoString* rebel_icall_RebelSharpDirs_ResConfigDir() {
    return GDMonoMarshal::mono_string_from_rebel(
        RebelSharpDirs::get_res_config_dir()
    );
}

MonoString* rebel_icall_RebelSharpDirs_ResTempDir() {
    return GDMonoMarshal::mono_string_from_rebel(
        RebelSharpDirs::get_res_temp_dir()
    );
}

MonoString* rebel_icall_RebelSharpDirs_ResTempAssembliesBaseDir() {
    return GDMonoMarshal::mono_string_from_rebel(
        RebelSharpDirs::get_res_temp_assemblies_base_dir()
    );
}

MonoString* rebel_icall_RebelSharpDirs_ResTempAssembliesDir() {
    return GDMonoMarshal::mono_string_from_rebel(
        RebelSharpDirs::get_res_temp_assemblies_dir()
    );
}

MonoString* rebel_icall_RebelSharpDirs_MonoUserDir() {
    return GDMonoMarshal::mono_string_from_rebel(
        RebelSharpDirs::get_mono_user_dir()
    );
}

MonoString* rebel_icall_RebelSharpDirs_MonoLogsDir() {
    return GDMonoMarshal::mono_string_from_rebel(
        RebelSharpDirs::get_mono_logs_dir()
    );
}

MonoString* rebel_icall_RebelSharpDirs_MonoSolutionsDir() {
#ifdef TOOLS_ENABLED
    return GDMonoMarshal::mono_string_from_rebel(
        RebelSharpDirs::get_mono_solutions_dir()
    );
#else
    return NULL;
#endif
}

MonoString* rebel_icall_RebelSharpDirs_BuildLogsDirs() {
#ifdef TOOLS_ENABLED
    return GDMonoMarshal::mono_string_from_rebel(
        RebelSharpDirs::get_build_logs_dir()
    );
#else
    return NULL;
#endif
}

MonoString* rebel_icall_RebelSharpDirs_ProjectSlnPath() {
#ifdef TOOLS_ENABLED
    return GDMonoMarshal::mono_string_from_rebel(
        RebelSharpDirs::get_project_sln_path()
    );
#else
    return NULL;
#endif
}

MonoString* rebel_icall_RebelSharpDirs_ProjectCsProjPath() {
#ifdef TOOLS_ENABLED
    return GDMonoMarshal::mono_string_from_rebel(
        RebelSharpDirs::get_project_csproj_path()
    );
#else
    return NULL;
#endif
}

MonoString* rebel_icall_RebelSharpDirs_DataEditorToolsDir() {
#ifdef TOOLS_ENABLED
    return GDMonoMarshal::mono_string_from_rebel(
        RebelSharpDirs::get_data_editor_tools_dir()
    );
#else
    return NULL;
#endif
}

MonoString* rebel_icall_RebelSharpDirs_DataEditorPrebuiltApiDir() {
#ifdef TOOLS_ENABLED
    return GDMonoMarshal::mono_string_from_rebel(
        RebelSharpDirs::get_data_editor_prebuilt_api_dir()
    );
#else
    return NULL;
#endif
}

MonoString* rebel_icall_RebelSharpDirs_DataMonoEtcDir() {
    return GDMonoMarshal::mono_string_from_rebel(
        RebelSharpDirs::get_data_mono_etc_dir()
    );
}

MonoString* rebel_icall_RebelSharpDirs_DataMonoLibDir() {
    return GDMonoMarshal::mono_string_from_rebel(
        RebelSharpDirs::get_data_mono_lib_dir()
    );
}

MonoString* rebel_icall_RebelSharpDirs_DataMonoBinDir() {
#ifdef WINDOWS_ENABLED
    return GDMonoMarshal::mono_string_from_rebel(
        RebelSharpDirs::get_data_mono_bin_dir()
    );
#else
    return NULL;
#endif
}

void rebel_icall_EditorProgress_Create(
    MonoString* p_task,
    MonoString* p_label,
    int32_t p_amount,
    MonoBoolean p_can_cancel
) {
    String task  = GDMonoMarshal::mono_string_to_rebel(p_task);
    String label = GDMonoMarshal::mono_string_to_rebel(p_label);
    EditorNode::progress_add_task(task, label, p_amount, (bool)p_can_cancel);
}

void rebel_icall_EditorProgress_Dispose(MonoString* p_task) {
    String task = GDMonoMarshal::mono_string_to_rebel(p_task);
    EditorNode::progress_end_task(task);
}

MonoBoolean rebel_icall_EditorProgress_Step(
    MonoString* p_task,
    MonoString* p_state,
    int32_t p_step,
    MonoBoolean p_force_refresh
) {
    String task  = GDMonoMarshal::mono_string_to_rebel(p_task);
    String state = GDMonoMarshal::mono_string_to_rebel(p_state);
    return EditorNode::progress_task_step(
        task,
        state,
        p_step,
        (bool)p_force_refresh
    );
}

int32_t rebel_icall_ScriptClassParser_ParseFile(
    MonoString* p_filepath,
    MonoObject* p_classes,
    MonoString** r_error_str
) {
    *r_error_str = NULL;

    String filepath = GDMonoMarshal::mono_string_to_rebel(p_filepath);

    ScriptClassParser scp;
    Error err = scp.parse_file(filepath);
    if (err == OK) {
        Array classes = GDMonoMarshal::mono_object_to_variant(p_classes);
        const Vector<ScriptClassParser::ClassDecl>& class_decls =
            scp.get_classes();

        for (int i = 0; i < class_decls.size(); i++) {
            const ScriptClassParser::ClassDecl& classDecl = class_decls[i];

            Dictionary classDeclDict;
            classDeclDict["name"]       = classDecl.name;
            classDeclDict["namespace"]  = classDecl.namespace_;
            classDeclDict["nested"]     = classDecl.nested;
            classDeclDict["base_count"] = classDecl.base.size();
            classes.push_back(classDeclDict);
        }
    } else {
        String error_str = scp.get_error();
        if (!error_str.empty()) {
            *r_error_str = GDMonoMarshal::mono_string_from_rebel(error_str);
        }
    }
    return err;
}

uint32_t rebel_icall_ExportPlugin_GetExportedAssemblyDependencies(
    MonoObject* p_initial_assemblies,
    MonoString* p_build_config,
    MonoString* p_custom_bcl_dir,
    MonoObject* r_assembly_dependencies
) {
    Dictionary initial_dependencies =
        GDMonoMarshal::mono_object_to_variant(p_initial_assemblies);
    String build_config = GDMonoMarshal::mono_string_to_rebel(p_build_config);
    String custom_bcl_dir =
        GDMonoMarshal::mono_string_to_rebel(p_custom_bcl_dir);
    Dictionary assembly_dependencies =
        GDMonoMarshal::mono_object_to_variant(r_assembly_dependencies);

    return RebelSharpExport::get_exported_assembly_dependencies(
        initial_dependencies,
        build_config,
        custom_bcl_dir,
        assembly_dependencies
    );
}

MonoString* rebel_icall_Internal_UpdateApiAssembliesFromPrebuilt(
    MonoString* p_config
) {
    String config = GDMonoMarshal::mono_string_to_rebel(p_config);
    String error_str =
        GDMono::get_singleton()->update_api_assemblies_from_prebuilt(config);
    return GDMonoMarshal::mono_string_from_rebel(error_str);
}

MonoString* rebel_icall_Internal_FullTemplatesDir() {
    String full_templates_dir =
        EditorSettings::get_singleton()->get_templates_dir().plus_file(
            VERSION_FULL_CONFIG
        );
    return GDMonoMarshal::mono_string_from_rebel(full_templates_dir);
}

MonoString* rebel_icall_Internal_SimplifyRebelPath(MonoString* p_path) {
    String path = GDMonoMarshal::mono_string_to_rebel(p_path);
    return GDMonoMarshal::mono_string_from_rebel(path.simplify_path());
}

MonoBoolean rebel_icall_Internal_IsOsxAppBundleInstalled(MonoString* p_bundle_id
) {
#ifdef MACOS_ENABLED
    String bundle_id = GDMonoMarshal::mono_string_to_rebel(p_bundle_id);
    return (MonoBoolean)macos_is_app_bundle_installed(bundle_id);
#else
    (void)p_bundle_id; // UNUSED
    return (MonoBoolean) false;
#endif
}

MonoBoolean rebel_icall_Internal_RebelIs32Bits() {
    return sizeof(void*) == 4;
}

MonoBoolean rebel_icall_Internal_RebelIsRealTDouble() {
#ifdef REAL_T_IS_DOUBLE
    return (MonoBoolean) true;
#else
    return (MonoBoolean) false;
#endif
}

void rebel_icall_Internal_RebelMainIteration() {
    Main::iteration();
}

uint64_t rebel_icall_Internal_GetCoreApiHash() {
    return ClassDB::get_api_hash(ClassDB::API_CORE);
}

uint64_t rebel_icall_Internal_GetEditorApiHash() {
    return ClassDB::get_api_hash(ClassDB::API_EDITOR);
}

MonoBoolean rebel_icall_Internal_IsAssembliesReloadingNeeded() {
#ifdef GD_MONO_HOT_RELOAD
    return (MonoBoolean)CSharpLanguage::get_singleton()
        ->is_assembly_reloading_needed();
#else
    return (MonoBoolean) false;
#endif
}

void rebel_icall_Internal_ReloadAssemblies(MonoBoolean p_soft_reload) {
#ifdef GD_MONO_HOT_RELOAD
    _RebelSharp::get_singleton()->call_deferred(
        "_reload_assemblies",
        (bool)p_soft_reload
    );
#endif
}

void rebel_icall_Internal_ScriptEditorDebuggerReloadScripts() {
    ScriptEditor::get_singleton()->get_debugger()->reload_scripts();
}

MonoBoolean rebel_icall_Internal_ScriptEditorEdit(
    MonoObject* p_resource,
    int32_t p_line,
    int32_t p_col,
    MonoBoolean p_grab_focus
) {
    Ref<Resource> resource = GDMonoMarshal::mono_object_to_variant(p_resource);
    return (MonoBoolean)ScriptEditor::get_singleton()
        ->edit(resource, p_line, p_col, (bool)p_grab_focus);
}

void rebel_icall_Internal_EditorNodeShowScriptScreen() {
    EditorNode::get_singleton()->call(
        "_editor_select",
        EditorNode::EDITOR_SCRIPT
    );
}

MonoObject* rebel_icall_Internal_GetScriptsMetadataOrNothing(
    MonoReflectionType* p_dict_reftype
) {
    Dictionary maybe_metadata =
        CSharpLanguage::get_singleton()->get_scripts_metadata_or_nothing();

    MonoType* dict_type = mono_reflection_type_get_type(p_dict_reftype);

    uint32_t type_encoding    = mono_type_get_type(dict_type);
    MonoClass* type_class_raw = mono_class_from_mono_type(dict_type);
    GDMonoClass* type_class =
        GDMono::get_singleton()->get_class(type_class_raw);

    return GDMonoMarshal::variant_to_mono_object(
        maybe_metadata,
        ManagedType(type_encoding, type_class)
    );
}

MonoString* rebel_icall_Internal_MonoWindowsInstallRoot() {
#ifdef WINDOWS_ENABLED
    String install_root_dir =
        GDMono::get_singleton()->get_mono_reg_info().install_root_dir;
    return GDMonoMarshal::mono_string_from_rebel(install_root_dir);
#else
    return NULL;
#endif
}

void rebel_icall_Internal_EditorRunPlay() {
    EditorNode::get_singleton()->run_play();
}

void rebel_icall_Internal_EditorRunStop() {
    EditorNode::get_singleton()->run_stop();
}

void rebel_icall_Internal_ScriptEditorDebugger_ReloadScripts() {
    ScriptEditorDebugger* sed = ScriptEditor::get_singleton()->get_debugger();
    if (sed) {
        sed->reload_scripts();
    }
}

MonoArray* rebel_icall_Internal_CodeCompletionRequest(
    int32_t p_kind,
    MonoString* p_script_file
) {
    String script_file = GDMonoMarshal::mono_string_to_rebel(p_script_file);
    PoolStringArray suggestions = gdmono::get_code_completion(
        (gdmono::CompletionKind)p_kind,
        script_file
    );
    return GDMonoMarshal::PoolStringArray_to_mono_array(suggestions);
}

float rebel_icall_Globals_EditorScale() {
    return EDSCALE;
}

MonoObject* rebel_icall_Globals_GlobalDef(
    MonoString* p_setting,
    MonoObject* p_default_value,
    MonoBoolean p_restart_if_changed
) {
    String setting = GDMonoMarshal::mono_string_to_rebel(p_setting);
    Variant default_value =
        GDMonoMarshal::mono_object_to_variant(p_default_value);
    Variant result =
        _GLOBAL_DEF(setting, default_value, (bool)p_restart_if_changed);
    return GDMonoMarshal::variant_to_mono_object(result);
}

MonoObject* rebel_icall_Globals_EditorDef(
    MonoString* p_setting,
    MonoObject* p_default_value,
    MonoBoolean p_restart_if_changed
) {
    String setting = GDMonoMarshal::mono_string_to_rebel(p_setting);
    Variant default_value =
        GDMonoMarshal::mono_object_to_variant(p_default_value);
    Variant result =
        _EDITOR_DEF(setting, default_value, (bool)p_restart_if_changed);
    return GDMonoMarshal::variant_to_mono_object(result);
}

MonoObject* rebel_icall_Globals_EditorShortcut(MonoString* p_setting) {
    String setting       = GDMonoMarshal::mono_string_to_rebel(p_setting);
    Ref<ShortCut> result = ED_GET_SHORTCUT(setting);
    return GDMonoMarshal::variant_to_mono_object(result);
}

MonoString* rebel_icall_Globals_TTR(MonoString* p_text) {
    String text = GDMonoMarshal::mono_string_to_rebel(p_text);
    return GDMonoMarshal::mono_string_from_rebel(TTR(text));
}

MonoString* rebel_icall_Utils_OS_GetPlatformName() {
    String os_name = OS::get_singleton()->get_name();
    return GDMonoMarshal::mono_string_from_rebel(os_name);
}

MonoBoolean rebel_icall_Utils_OS_UnixFileHasExecutableAccess(
    MonoString* p_file_path
) {
#ifdef UNIX_ENABLED
    String file_path = GDMonoMarshal::mono_string_to_rebel(p_file_path);
    return access(file_path.utf8().get_data(), X_OK) == 0;
#else
    ERR_FAIL_V(false);
#endif
}

void register_editor_internal_calls() {
    // RebelSharpDirs
    GDMonoUtils::add_internal_call(
        "RebelTools.Internals.RebelSharpDirs::internal_ResDataDir",
        rebel_icall_RebelSharpDirs_ResDataDir
    );
    GDMonoUtils::add_internal_call(
        "RebelTools.Internals.RebelSharpDirs::internal_ResMetadataDir",
        rebel_icall_RebelSharpDirs_ResMetadataDir
    );
    GDMonoUtils::add_internal_call(
        "RebelTools.Internals.RebelSharpDirs::internal_ResAssembliesBaseDir",
        rebel_icall_RebelSharpDirs_ResAssembliesBaseDir
    );
    GDMonoUtils::add_internal_call(
        "RebelTools.Internals.RebelSharpDirs::internal_ResAssembliesDir",
        rebel_icall_RebelSharpDirs_ResAssembliesDir
    );
    GDMonoUtils::add_internal_call(
        "RebelTools.Internals.RebelSharpDirs::internal_ResConfigDir",
        rebel_icall_RebelSharpDirs_ResConfigDir
    );
    GDMonoUtils::add_internal_call(
        "RebelTools.Internals.RebelSharpDirs::internal_ResTempDir",
        rebel_icall_RebelSharpDirs_ResTempDir
    );
    GDMonoUtils::add_internal_call(
        "RebelTools.Internals.RebelSharpDirs::internal_"
        "ResTempAssembliesBaseDir",
        rebel_icall_RebelSharpDirs_ResTempAssembliesBaseDir
    );
    GDMonoUtils::add_internal_call(
        "RebelTools.Internals.RebelSharpDirs::internal_ResTempAssembliesDir",
        rebel_icall_RebelSharpDirs_ResTempAssembliesDir
    );
    GDMonoUtils::add_internal_call(
        "RebelTools.Internals.RebelSharpDirs::internal_MonoUserDir",
        rebel_icall_RebelSharpDirs_MonoUserDir
    );
    GDMonoUtils::add_internal_call(
        "RebelTools.Internals.RebelSharpDirs::internal_MonoLogsDir",
        rebel_icall_RebelSharpDirs_MonoLogsDir
    );
    GDMonoUtils::add_internal_call(
        "RebelTools.Internals.RebelSharpDirs::internal_MonoSolutionsDir",
        rebel_icall_RebelSharpDirs_MonoSolutionsDir
    );
    GDMonoUtils::add_internal_call(
        "RebelTools.Internals.RebelSharpDirs::internal_BuildLogsDirs",
        rebel_icall_RebelSharpDirs_BuildLogsDirs
    );
    GDMonoUtils::add_internal_call(
        "RebelTools.Internals.RebelSharpDirs::internal_ProjectSlnPath",
        rebel_icall_RebelSharpDirs_ProjectSlnPath
    );
    GDMonoUtils::add_internal_call(
        "RebelTools.Internals.RebelSharpDirs::internal_ProjectCsProjPath",
        rebel_icall_RebelSharpDirs_ProjectCsProjPath
    );
    GDMonoUtils::add_internal_call(
        "RebelTools.Internals.RebelSharpDirs::internal_DataEditorToolsDir",
        rebel_icall_RebelSharpDirs_DataEditorToolsDir
    );
    GDMonoUtils::add_internal_call(
        "RebelTools.Internals.RebelSharpDirs::internal_"
        "DataEditorPrebuiltApiDir",
        rebel_icall_RebelSharpDirs_DataEditorPrebuiltApiDir
    );
    GDMonoUtils::add_internal_call(
        "RebelTools.Internals.RebelSharpDirs::internal_DataMonoEtcDir",
        rebel_icall_RebelSharpDirs_DataMonoEtcDir
    );
    GDMonoUtils::add_internal_call(
        "RebelTools.Internals.RebelSharpDirs::internal_DataMonoLibDir",
        rebel_icall_RebelSharpDirs_DataMonoLibDir
    );
    GDMonoUtils::add_internal_call(
        "RebelTools.Internals.RebelSharpDirs::internal_DataMonoBinDir",
        rebel_icall_RebelSharpDirs_DataMonoBinDir
    );

    // EditorProgress
    GDMonoUtils::add_internal_call(
        "RebelTools.Internals.EditorProgress::internal_Create",
        rebel_icall_EditorProgress_Create
    );
    GDMonoUtils::add_internal_call(
        "RebelTools.Internals.EditorProgress::internal_Dispose",
        rebel_icall_EditorProgress_Dispose
    );
    GDMonoUtils::add_internal_call(
        "RebelTools.Internals.EditorProgress::internal_Step",
        rebel_icall_EditorProgress_Step
    );

    // ScriptClassParser
    GDMonoUtils::add_internal_call(
        "RebelTools.Internals.ScriptClassParser::internal_ParseFile",
        rebel_icall_ScriptClassParser_ParseFile
    );

    // ExportPlugin
    GDMonoUtils::add_internal_call(
        "RebelTools.Export.ExportPlugin::internal_"
        "GetExportedAssemblyDependencies",
        rebel_icall_ExportPlugin_GetExportedAssemblyDependencies
    );

    // Internals
    GDMonoUtils::add_internal_call(
        "RebelTools.Internals.Internal::internal_"
        "UpdateApiAssembliesFromPrebuilt",
        rebel_icall_Internal_UpdateApiAssembliesFromPrebuilt
    );
    GDMonoUtils::add_internal_call(
        "RebelTools.Internals.Internal::internal_FullTemplatesDir",
        rebel_icall_Internal_FullTemplatesDir
    );
    GDMonoUtils::add_internal_call(
        "RebelTools.Internals.Internal::internal_SimplifyRebelPath",
        rebel_icall_Internal_SimplifyRebelPath
    );
    GDMonoUtils::add_internal_call(
        "RebelTools.Internals.Internal::internal_IsOsxAppBundleInstalled",
        rebel_icall_Internal_IsOsxAppBundleInstalled
    );
    GDMonoUtils::add_internal_call(
        "RebelTools.Internals.Internal::internal_RebelIs32Bits",
        rebel_icall_Internal_RebelIs32Bits
    );
    GDMonoUtils::add_internal_call(
        "RebelTools.Internals.Internal::internal_RebelIsRealTDouble",
        rebel_icall_Internal_RebelIsRealTDouble
    );
    GDMonoUtils::add_internal_call(
        "RebelTools.Internals.Internal::internal_RebelMainIteration",
        rebel_icall_Internal_RebelMainIteration
    );
    GDMonoUtils::add_internal_call(
        "RebelTools.Internals.Internal::internal_GetCoreApiHash",
        rebel_icall_Internal_GetCoreApiHash
    );
    GDMonoUtils::add_internal_call(
        "RebelTools.Internals.Internal::internal_GetEditorApiHash",
        rebel_icall_Internal_GetEditorApiHash
    );
    GDMonoUtils::add_internal_call(
        "RebelTools.Internals.Internal::internal_IsAssembliesReloadingNeeded",
        rebel_icall_Internal_IsAssembliesReloadingNeeded
    );
    GDMonoUtils::add_internal_call(
        "RebelTools.Internals.Internal::internal_ReloadAssemblies",
        rebel_icall_Internal_ReloadAssemblies
    );
    GDMonoUtils::add_internal_call(
        "RebelTools.Internals.Internal::internal_"
        "ScriptEditorDebuggerReloadScripts",
        rebel_icall_Internal_ScriptEditorDebuggerReloadScripts
    );
    GDMonoUtils::add_internal_call(
        "RebelTools.Internals.Internal::internal_ScriptEditorEdit",
        rebel_icall_Internal_ScriptEditorEdit
    );
    GDMonoUtils::add_internal_call(
        "RebelTools.Internals.Internal::internal_EditorNodeShowScriptScreen",
        rebel_icall_Internal_EditorNodeShowScriptScreen
    );
    GDMonoUtils::add_internal_call(
        "RebelTools.Internals.Internal::internal_GetScriptsMetadataOrNothing",
        rebel_icall_Internal_GetScriptsMetadataOrNothing
    );
    GDMonoUtils::add_internal_call(
        "RebelTools.Internals.Internal::internal_MonoWindowsInstallRoot",
        rebel_icall_Internal_MonoWindowsInstallRoot
    );
    GDMonoUtils::add_internal_call(
        "RebelTools.Internals.Internal::internal_EditorRunPlay",
        rebel_icall_Internal_EditorRunPlay
    );
    GDMonoUtils::add_internal_call(
        "RebelTools.Internals.Internal::internal_EditorRunStop",
        rebel_icall_Internal_EditorRunStop
    );
    GDMonoUtils::add_internal_call(
        "RebelTools.Internals.Internal::internal_ScriptEditorDebugger_"
        "ReloadScripts",
        rebel_icall_Internal_ScriptEditorDebugger_ReloadScripts
    );
    GDMonoUtils::add_internal_call(
        "RebelTools.Internals.Internal::internal_CodeCompletionRequest",
        rebel_icall_Internal_CodeCompletionRequest
    );

    // Globals
    GDMonoUtils::add_internal_call(
        "RebelTools.Internals.Globals::internal_EditorScale",
        rebel_icall_Globals_EditorScale
    );
    GDMonoUtils::add_internal_call(
        "RebelTools.Internals.Globals::internal_GlobalDef",
        rebel_icall_Globals_GlobalDef
    );
    GDMonoUtils::add_internal_call(
        "RebelTools.Internals.Globals::internal_EditorDef",
        rebel_icall_Globals_EditorDef
    );
    GDMonoUtils::add_internal_call(
        "RebelTools.Internals.Globals::internal_EditorShortcut",
        rebel_icall_Globals_EditorShortcut
    );
    GDMonoUtils::add_internal_call(
        "RebelTools.Internals.Globals::internal_TTR",
        rebel_icall_Globals_TTR
    );

    // Utils.OS
    GDMonoUtils::add_internal_call(
        "RebelTools.Utils.OS::GetPlatformName",
        rebel_icall_Utils_OS_GetPlatformName
    );
    GDMonoUtils::add_internal_call(
        "RebelTools.Utils.OS::UnixFileHasExecutableAccess",
        rebel_icall_Utils_OS_UnixFileHasExecutableAccess
    );
}
