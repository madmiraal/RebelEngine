// SPDX-FileCopyrightText: 2023 Rebel Engine contributors
// SPDX-FileCopyrightText: 2014-2022 Godot Engine contributors
// SPDX-FileCopyrightText: 2007-2014 Juan Linietsky, Ariel Manzur
//
// SPDX-License-Identifier: MIT

#include "export.h"

#include "core/io/marshalls.h"
#include "core/io/resource_saver.h"
#include "core/io/zip_io.h"
#include "core/os/dir_access.h"
#include "core/os/file_access.h"
#include "core/os/os.h"
#include "core/project_settings.h"
#include "core/version.h"
#include "editor/editor_export.h"
#include "editor/editor_node.h"
#include "editor/editor_settings.h"
#include "platforms/macos/logo.gen.h"

#include <sys/stat.h>

class MacOSEditorExportPlatform : public EditorExportPlatform {
    GDCLASS(MacOSEditorExportPlatform, EditorExportPlatform);

    int version_code;

    Ref<ImageTexture> logo;

    void _fix_plist(
        const Ref<EditorExportPreset>& p_preset,
        Vector<uint8_t>& plist,
        const String& p_binary
    );
    void _make_icon(const Ref<Image>& p_icon, Vector<uint8_t>& p_data);

    Error _notarize(
        const Ref<EditorExportPreset>& p_preset,
        const String& p_path
    );
    Error _code_sign(
        const Ref<EditorExportPreset>& p_preset,
        const String& p_path,
        const String& p_ent_path
    );
    Error _create_dmg(
        const String& p_dmg_path,
        const String& p_pkg_name,
        const String& p_app_path_name
    );
    void _zip_folder_recursive(
        zipFile& p_zip,
        const String& p_root_path,
        const String& p_folder,
        const String& p_pkg_name
    );

#ifdef MACOS_ENABLED
    bool use_codesign() const {
        return true;
    }

    bool use_dmg() const {
        return true;
    }
#else
    bool use_codesign() const {
        return false;
    }

    bool use_dmg() const {
        return false;
    }
#endif
    bool is_package_name_valid(
        const String& p_package,
        String* r_error = nullptr
    ) const {
        String pname = p_package;

        if (pname.length() == 0) {
            if (r_error) {
                *r_error = TTR("Identifier is missing.");
            }
            return false;
        }

        for (int i = 0; i < pname.length(); i++) {
            char32_t c = pname[i];
            if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')
                  || (c >= '0' && c <= '9') || c == '-' || c == '.')) {
                if (r_error) {
                    *r_error = vformat(
                        TTR("The character '%s' is not allowed in Identifier."),
                        String::chr(c)
                    );
                }
                return false;
            }
        }

        return true;
    }

protected:
    virtual void get_preset_features(
        const Ref<EditorExportPreset>& p_preset,
        List<String>* r_features
    );
    virtual void get_export_options(List<ExportOption>* r_options);

public:
    virtual String get_name() const {
        return "MacOS";
    }

    virtual String get_os_name() const {
        return "MacOS";
    }

    virtual Ref<Texture> get_logo() const {
        return logo;
    }

    virtual List<String> get_binary_extensions(
        const Ref<EditorExportPreset>& p_preset
    ) const {
        List<String> list;
        if (use_dmg()) {
            list.push_back("dmg");
        }
        list.push_back("zip");
        return list;
    }

    virtual Error export_project(
        const Ref<EditorExportPreset>& p_preset,
        bool p_debug,
        const String& p_path,
        int p_flags = 0
    );

    virtual bool can_export(
        const Ref<EditorExportPreset>& p_preset,
        String& r_error,
        bool& r_missing_templates
    ) const;

    virtual void get_platform_features(List<String>* r_features) {
        r_features->push_back("pc");
        r_features->push_back("s3tc");
        r_features->push_back("MacOS");
    }

    virtual void resolve_platform_feature_priorities(
        const Ref<EditorExportPreset>& p_preset,
        Set<String>& p_features
    ) {}

    MacOSEditorExportPlatform();
    ~MacOSEditorExportPlatform();
};

void MacOSEditorExportPlatform::get_preset_features(
    const Ref<EditorExportPreset>& p_preset,
    List<String>* r_features
) {
    if (p_preset->get("texture_format/s3tc")) {
        r_features->push_back("s3tc");
    }
    if (p_preset->get("texture_format/etc")) {
        r_features->push_back("etc");
    }
    if (p_preset->get("texture_format/etc2")) {
        r_features->push_back("etc2");
    }

    r_features->push_back("64");
}

void MacOSEditorExportPlatform::get_export_options(List<ExportOption>* r_options
) {
    r_options->push_back(ExportOption(
        PropertyInfo(
            Variant::STRING,
            "custom_template/debug",
            PROPERTY_HINT_GLOBAL_FILE,
            "*.zip"
        ),
        ""
    ));
    r_options->push_back(ExportOption(
        PropertyInfo(
            Variant::STRING,
            "custom_template/release",
            PROPERTY_HINT_GLOBAL_FILE,
            "*.zip"
        ),
        ""
    ));

    r_options->push_back(ExportOption(
        PropertyInfo(
            Variant::STRING,
            "application/name",
            PROPERTY_HINT_PLACEHOLDER_TEXT,
            "Game Name"
        ),
        ""
    ));
    r_options->push_back(ExportOption(
        PropertyInfo(Variant::STRING, "application/info"),
        "Made with Rebel Engine"
    ));
    r_options->push_back(ExportOption(
        PropertyInfo(
            Variant::STRING,
            "application/icon",
            PROPERTY_HINT_FILE,
            "*.png,*.icns"
        ),
        ""
    ));
    r_options->push_back(ExportOption(
        PropertyInfo(
            Variant::STRING,
            "application/identifier",
            PROPERTY_HINT_PLACEHOLDER_TEXT,
            "com.example.game"
        ),
        ""
    ));
    r_options->push_back(
        ExportOption(PropertyInfo(Variant::STRING, "application/signature"), "")
    );
    r_options->push_back(ExportOption(
        PropertyInfo(
            Variant::STRING,
            "application/app_category",
            PROPERTY_HINT_ENUM,
            "Business,Developer-tools,Education,Entertainment,Finance,Games,"
            "Action-games,Adventure-games,Arcade-games,Board-games,Card-games,"
            "Casino-games,Dice-games,Educational-games,Family-games,Kids-games,"
            "Music-games,Puzzle-games,Racing-games,Role-playing-games,"
            "Simulation-games,Sports-games,Strategy-games,Trivia-games,Word-"
            "games,Graphics-design,Healthcare-fitness,Lifestyle,Medical,Music,"
            "News,Photography,Productivity,Reference,Social-networking,Sports,"
            "Travel,Utilities,Video,Weather"
        ),
        "Games"
    ));
    r_options->push_back(ExportOption(
        PropertyInfo(Variant::STRING, "application/short_version"),
        "1.0"
    ));
    r_options->push_back(ExportOption(
        PropertyInfo(Variant::STRING, "application/version"),
        "1.0"
    ));
    r_options->push_back(
        ExportOption(PropertyInfo(Variant::STRING, "application/copyright"), "")
    );
    r_options->push_back(
        ExportOption(PropertyInfo(Variant::BOOL, "display/high_res"), false)
    );
    r_options->push_back(ExportOption(
        PropertyInfo(
            Variant::STRING,
            "privacy/camera_usage_description",
            PROPERTY_HINT_PLACEHOLDER_TEXT,
            "Provide a message if you need to use the camera"
        ),
        ""
    ));
    r_options->push_back(ExportOption(
        PropertyInfo(
            Variant::STRING,
            "privacy/microphone_usage_description",
            PROPERTY_HINT_PLACEHOLDER_TEXT,
            "Provide a message if you need to use the microphone"
        ),
        ""
    ));

#ifdef MACOS_ENABLED
    r_options->push_back(
        ExportOption(PropertyInfo(Variant::BOOL, "codesign/enable"), true)
    );
    r_options->push_back(ExportOption(
        PropertyInfo(
            Variant::STRING,
            "codesign/identity",
            PROPERTY_HINT_PLACEHOLDER_TEXT,
            "Type: Name (ID)"
        ),
        ""
    ));
    r_options->push_back(
        ExportOption(PropertyInfo(Variant::BOOL, "codesign/timestamp"), true)
    );
    r_options->push_back(ExportOption(
        PropertyInfo(Variant::BOOL, "codesign/hardened_runtime"),
        true
    ));
    r_options->push_back(ExportOption(
        PropertyInfo(Variant::BOOL, "codesign/replace_existing_signature"),
        true
    ));
    r_options->push_back(ExportOption(
        PropertyInfo(
            Variant::STRING,
            "codesign/entitlements/custom_file",
            PROPERTY_HINT_GLOBAL_FILE,
            "*.plist"
        ),
        ""
    ));

    if (!Engine::get_singleton()->has_singleton("GodotSharp")) {
        // These entitlements are required to run managed code, and are always
        // enabled in Mono builds.
        r_options->push_back(ExportOption(
            PropertyInfo(
                Variant::BOOL,
                "codesign/entitlements/allow_jit_code_execution"
            ),
            false
        ));
        r_options->push_back(ExportOption(
            PropertyInfo(
                Variant::BOOL,
                "codesign/entitlements/allow_unsigned_executable_memory"
            ),
            false
        ));
        r_options->push_back(ExportOption(
            PropertyInfo(
                Variant::BOOL,
                "codesign/entitlements/allow_dyld_environment_variables"
            ),
            false
        ));
    }

    r_options->push_back(ExportOption(
        PropertyInfo(
            Variant::BOOL,
            "codesign/entitlements/disable_library_validation"
        ),
        false
    ));
    r_options->push_back(ExportOption(
        PropertyInfo(Variant::BOOL, "codesign/entitlements/audio_input"),
        false
    ));
    r_options->push_back(ExportOption(
        PropertyInfo(Variant::BOOL, "codesign/entitlements/camera"),
        false
    ));
    r_options->push_back(ExportOption(
        PropertyInfo(Variant::BOOL, "codesign/entitlements/location"),
        false
    ));
    r_options->push_back(ExportOption(
        PropertyInfo(Variant::BOOL, "codesign/entitlements/address_book"),
        false
    ));
    r_options->push_back(ExportOption(
        PropertyInfo(Variant::BOOL, "codesign/entitlements/calendars"),
        false
    ));
    r_options->push_back(ExportOption(
        PropertyInfo(Variant::BOOL, "codesign/entitlements/photos_library"),
        false
    ));
    r_options->push_back(ExportOption(
        PropertyInfo(Variant::BOOL, "codesign/entitlements/apple_events"),
        false
    ));
    r_options->push_back(ExportOption(
        PropertyInfo(Variant::BOOL, "codesign/entitlements/debugging"),
        false
    ));

    r_options->push_back(ExportOption(
        PropertyInfo(
            Variant::BOOL,
            "codesign/entitlements/app_sandbox/enabled"
        ),
        false
    ));
    r_options->push_back(ExportOption(
        PropertyInfo(
            Variant::BOOL,
            "codesign/entitlements/app_sandbox/network_server"
        ),
        false
    ));
    r_options->push_back(ExportOption(
        PropertyInfo(
            Variant::BOOL,
            "codesign/entitlements/app_sandbox/network_client"
        ),
        false
    ));
    r_options->push_back(ExportOption(
        PropertyInfo(
            Variant::BOOL,
            "codesign/entitlements/app_sandbox/device_usb"
        ),
        false
    ));
    r_options->push_back(ExportOption(
        PropertyInfo(
            Variant::BOOL,
            "codesign/entitlements/app_sandbox/device_bluetooth"
        ),
        false
    ));
    r_options->push_back(ExportOption(
        PropertyInfo(
            Variant::INT,
            "codesign/entitlements/app_sandbox/files_downloads",
            PROPERTY_HINT_ENUM,
            "No,Read-only,Read-write"
        ),
        0
    ));
    r_options->push_back(ExportOption(
        PropertyInfo(
            Variant::INT,
            "codesign/entitlements/app_sandbox/files_pictures",
            PROPERTY_HINT_ENUM,
            "No,Read-only,Read-write"
        ),
        0
    ));
    r_options->push_back(ExportOption(
        PropertyInfo(
            Variant::INT,
            "codesign/entitlements/app_sandbox/files_music",
            PROPERTY_HINT_ENUM,
            "No,Read-only,Read-write"
        ),
        0
    ));
    r_options->push_back(ExportOption(
        PropertyInfo(
            Variant::INT,
            "codesign/entitlements/app_sandbox/files_movies",
            PROPERTY_HINT_ENUM,
            "No,Read-only,Read-write"
        ),
        0
    ));

    r_options->push_back(ExportOption(
        PropertyInfo(Variant::POOL_STRING_ARRAY, "codesign/custom_options"),
        PoolStringArray()
    ));

    r_options->push_back(
        ExportOption(PropertyInfo(Variant::BOOL, "notarization/enable"), false)
    );
    r_options->push_back(ExportOption(
        PropertyInfo(
            Variant::STRING,
            "notarization/apple_id_name",
            PROPERTY_HINT_PLACEHOLDER_TEXT,
            "Apple ID email"
        ),
        ""
    ));
    r_options->push_back(ExportOption(
        PropertyInfo(
            Variant::STRING,
            "notarization/apple_id_password",
            PROPERTY_HINT_PLACEHOLDER_TEXT,
            "Enable two-factor authentication and provide app-specific password"
        ),
        ""
    ));
    r_options->push_back(ExportOption(
        PropertyInfo(
            Variant::STRING,
            "notarization/apple_team_id",
            PROPERTY_HINT_PLACEHOLDER_TEXT,
            "Provide team ID if your Apple ID belongs to multiple teams"
        ),
        ""
    ));
#endif

    r_options->push_back(
        ExportOption(PropertyInfo(Variant::BOOL, "texture_format/s3tc"), true)
    );
    r_options->push_back(
        ExportOption(PropertyInfo(Variant::BOOL, "texture_format/etc"), false)
    );
    r_options->push_back(
        ExportOption(PropertyInfo(Variant::BOOL, "texture_format/etc2"), false)
    );
}

void _rgba8_to_packbits_encode(
    int p_ch,
    int p_size,
    PoolVector<uint8_t>& p_source,
    Vector<uint8_t>& p_dest
) {
    int src_len = p_size * p_size;

    Vector<uint8_t> result;
    result.resize(src_len * 1.25); // temp vector for rle encoded data, make it
                                   // 25% larger for worst case scenario
    int res_size = 0;

    uint8_t buf[128];
    int buf_size = 0;

    int i = 0;
    while (i < src_len) {
        uint8_t cur = p_source.read()[i * 4 + p_ch];

        if (i < src_len - 2) {
            if ((p_source.read()[(i + 1) * 4 + p_ch] == cur)
                && (p_source.read()[(i + 2) * 4 + p_ch] == cur)) {
                if (buf_size > 0) {
                    result.write[res_size++] = (uint8_t)(buf_size - 1);
                    memcpy(&result.write[res_size], &buf, buf_size);
                    res_size += buf_size;
                    buf_size  = 0;
                }

                uint8_t lim  = i + 130 >= src_len ? src_len - i - 1 : 130;
                bool hit_lim = true;

                for (int j = 3; j <= lim; j++) {
                    if (p_source.read()[(i + j) * 4 + p_ch] != cur) {
                        hit_lim                  = false;
                        i                        = i + j - 1;
                        result.write[res_size++] = (uint8_t)(j - 3 + 0x80);
                        result.write[res_size++] = cur;
                        break;
                    }
                }
                if (hit_lim) {
                    result.write[res_size++] = (uint8_t)(lim - 3 + 0x80);
                    result.write[res_size++] = cur;
                    i                        = i + lim;
                }
            } else {
                buf[buf_size++] = cur;
                if (buf_size == 128) {
                    result.write[res_size++] = (uint8_t)(buf_size - 1);
                    memcpy(&result.write[res_size], &buf, buf_size);
                    res_size += buf_size;
                    buf_size  = 0;
                }
            }
        } else {
            buf[buf_size++]          = cur;
            result.write[res_size++] = (uint8_t)(buf_size - 1);
            memcpy(&result.write[res_size], &buf, buf_size);
            res_size += buf_size;
            buf_size  = 0;
        }

        i++;
    }

    int ofs = p_dest.size();
    p_dest.resize(p_dest.size() + res_size);
    memcpy(&p_dest.write[ofs], result.ptr(), res_size);
}

void MacOSEditorExportPlatform::_make_icon(
    const Ref<Image>& p_icon,
    Vector<uint8_t>& p_data
) {
    Ref<ImageTexture> it = memnew(ImageTexture);

    Vector<uint8_t> data;

    data.resize(8);
    data.write[0] = 'i';
    data.write[1] = 'c';
    data.write[2] = 'n';
    data.write[3] = 's';

    struct MacOSIconInfo {
        const char* name;
        const char* mask_name;
        bool is_png;
        int size;
    };

    static const MacOSIconInfo icon_infos[] = {
        {"ic10", "",     true,  1024
        }, // 1024x1024 32-bit PNG and 512x512@2x 32-bit "retina" PNG
        {"ic09", "",     true,  512 }, // 512×512 32-bit PNG
        {"ic14", "",     true,  512 }, // 256x256@2x 32-bit "retina" PNG
        {"ic08", "",     true,  256 }, // 256×256 32-bit PNG
        {"ic13", "",     true,  256 }, // 128x128@2x 32-bit "retina" PNG
        {"ic07", "",     true,  128 }, // 128x128 32-bit PNG
        {"ic12", "",     true,  64  }, // 32x32@2x 32-bit "retina" PNG
        {"ic11", "",     true,  32  }, // 16x16@2x 32-bit "retina" PNG
        {"il32", "l8mk", false, 32
        }, // 32x32 24-bit RLE + 8-bit uncompressed mask
        {"is32", "s8mk", false, 16
        }  // 16x16 24-bit RLE + 8-bit uncompressed mask
    };

    for (uint64_t i = 0; i < (sizeof(icon_infos) / sizeof(icon_infos[0]));
         ++i) {
        Ref<Image> copy = p_icon; // does this make sense? doesn't this just
                                  // increase the reference count instead of
                                  // making a copy? Do we even need a copy?
        copy->convert(Image::FORMAT_RGBA8);
        copy->resize(icon_infos[i].size, icon_infos[i].size);

        if (icon_infos[i].is_png) {
            // Encode PNG icon.
            it->create_from_image(copy);
            String path =
                EditorSettings::get_singleton()->get_cache_dir().plus_file(
                    "icon.png"
                );
            ResourceSaver::save(path, it);

            FileAccess* f = FileAccess::open(path, FileAccess::READ);
            if (!f) {
                // Clean up generated file.
                DirAccess::remove_file_or_error(path);
                ERR_FAIL();
            }

            int ofs      = data.size();
            uint64_t len = f->get_len();
            data.resize(data.size() + len + 8);
            f->get_buffer(&data.write[ofs + 8], len);
            memdelete(f);
            len += 8;
            len  = BSWAP32(len);
            memcpy(&data.write[ofs], icon_infos[i].name, 4);
            encode_uint32(len, &data.write[ofs + 4]);

            // Clean up generated file.
            DirAccess::remove_file_or_error(path);

        } else {
            PoolVector<uint8_t> src_data = copy->get_data();

            // encode 24bit RGB RLE icon
            {
                int ofs = data.size();
                data.resize(data.size() + 8);

                _rgba8_to_packbits_encode(
                    0,
                    icon_infos[i].size,
                    src_data,
                    data
                ); // encode R
                _rgba8_to_packbits_encode(
                    1,
                    icon_infos[i].size,
                    src_data,
                    data
                ); // encode G
                _rgba8_to_packbits_encode(
                    2,
                    icon_infos[i].size,
                    src_data,
                    data
                ); // encode B

                int len = data.size() - ofs;
                len     = BSWAP32(len);
                memcpy(&data.write[ofs], icon_infos[i].name, 4);
                encode_uint32(len, &data.write[ofs + 4]);
            }

            // encode 8bit mask uncompressed icon
            {
                int ofs = data.size();
                int len = copy->get_width() * copy->get_height();
                data.resize(data.size() + len + 8);

                for (int j = 0; j < len; j++) {
                    data.write[ofs + 8 + j] = src_data.read()[j * 4 + 3];
                }
                len += 8;
                len  = BSWAP32(len);
                memcpy(&data.write[ofs], icon_infos[i].mask_name, 4);
                encode_uint32(len, &data.write[ofs + 4]);
            }
        }
    }

    uint32_t total_len = data.size();
    total_len          = BSWAP32(total_len);
    encode_uint32(total_len, &data.write[4]);

    p_data = data;
}

void MacOSEditorExportPlatform::_fix_plist(
    const Ref<EditorExportPreset>& p_preset,
    Vector<uint8_t>& plist,
    const String& p_binary
) {
    String str;
    String strnew;
    str.parse_utf8((const char*)plist.ptr(), plist.size());
    Vector<String> lines = str.split("\n");
    for (int i = 0; i < lines.size(); i++) {
        if (lines[i].find("$binary") != -1) {
            strnew += lines[i].replace("$binary", p_binary) + "\n";
        } else if (lines[i].find("$name") != -1) {
            strnew += lines[i].replace("$name", p_binary) + "\n";
        } else if (lines[i].find("$info") != -1) {
            strnew +=
                lines[i].replace("$info", p_preset->get("application/info"))
                + "\n";
        } else if (lines[i].find("$identifier") != -1) {
            strnew += lines[i].replace(
                          "$identifier",
                          p_preset->get("application/identifier")
                      )
                    + "\n";
        } else if (lines[i].find("$short_version") != -1) {
            strnew += lines[i].replace(
                          "$short_version",
                          p_preset->get("application/short_version")
                      )
                    + "\n";
        } else if (lines[i].find("$version") != -1) {
            strnew += lines[i].replace(
                          "$version",
                          p_preset->get("application/version")
                      )
                    + "\n";
        } else if (lines[i].find("$signature") != -1) {
            strnew += lines[i].replace(
                          "$signature",
                          p_preset->get("application/signature")
                      )
                    + "\n";
        } else if (lines[i].find("$app_category") != -1) {
            String cat = p_preset->get("application/app_category");
            strnew += lines[i].replace("$app_category", cat.to_lower()) + "\n";
        } else if (lines[i].find("$copyright") != -1) {
            strnew += lines[i].replace(
                          "$copyright",
                          p_preset->get("application/copyright")
                      )
                    + "\n";
        } else if (lines[i].find("$highres") != -1) {
            strnew +=
                lines[i].replace(
                    "$highres",
                    p_preset->get("display/high_res") ? "<true/>" : "<false/>"
                )
                + "\n";
        } else if (lines[i].find("$camera_usage_description") != -1) {
            String description =
                p_preset->get("privacy/camera_usage_description");
            strnew += lines[i].replace("$camera_usage_description", description)
                    + "\n";
        } else if (lines[i].find("$microphone_usage_description") != -1) {
            String description =
                p_preset->get("privacy/microphone_usage_description");
            strnew +=
                lines[i].replace("$microphone_usage_description", description)
                + "\n";
        } else {
            strnew += lines[i] + "\n";
        }
    }

    CharString cs = strnew.utf8();
    plist.resize(cs.size() - 1);
    for (int i = 0; i < cs.size() - 1; i++) {
        plist.write[i] = cs[i];
    }
}

// If we're running the MacOS version of the Rebel Editor we'll:
// - export our application bundle to a temporary folder
// - attempt to code sign it
// - and then wrap it up in a DMG

Error MacOSEditorExportPlatform::_notarize(
    const Ref<EditorExportPreset>& p_preset,
    const String& p_path
) {
#ifdef MACOS_ENABLED
    List<String> args;

    args.push_back("altool");
    args.push_back("--notarize-app");

    args.push_back("--primary-bundle-id");
    args.push_back(p_preset->get("application/identifier"));

    args.push_back("--username");
    args.push_back(p_preset->get("notarization/apple_id_name"));

    args.push_back("--password");
    args.push_back(p_preset->get("notarization/apple_id_password"));

    args.push_back("--type");
    args.push_back("macos");

    if (p_preset->get("notarization/apple_team_id")) {
        args.push_back("--asc-provider");
        args.push_back(p_preset->get("notarization/apple_team_id"));
    }

    args.push_back("--file");
    args.push_back(p_path);

    String str;
    Error err = OS::get_singleton()
                    ->execute("xcrun", args, true, NULL, &str, NULL, true);
    ERR_FAIL_COND_V(err != OK, err);

    print_line("altool (" + p_path + "):\n" + str);
    if (str.find("RequestUUID") == -1) {
        EditorNode::add_io_error("altool: " + str);
        return FAILED;
    } else {
        print_line(
            "Note: The notarization process generally takes less than an hour. "
            "When the process is completed, you'll receive an email."
        );
        print_line(
            "      You can check progress manually by opening a Terminal and "
            "running the following command:"
        );
        print_line(
            "      \"xcrun altool --notarization-history 0 -u <your email> -p "
            "<app-specific pwd>\""
        );
    }

#endif

    return OK;
}

Error MacOSEditorExportPlatform::_code_sign(
    const Ref<EditorExportPreset>& p_preset,
    const String& p_path,
    const String& p_ent_path
) {
#ifdef MACOS_ENABLED
    List<String> args;

    if (p_preset->get("codesign/timestamp")) {
        args.push_back("--timestamp");
    }
    if (p_preset->get("codesign/hardened_runtime")) {
        args.push_back("--options");
        args.push_back("runtime");
    }

    if (p_path.get_extension() != "dmg") {
        args.push_back("--entitlements");
        args.push_back(p_ent_path);
    }

    PoolStringArray user_args = p_preset->get("codesign/custom_options");
    for (int i = 0; i < user_args.size(); i++) {
        String user_arg = user_args[i].strip_edges();
        if (!user_arg.empty()) {
            args.push_back(user_arg);
        }
    }

    args.push_back("-s");
    if (p_preset->get("codesign/identity") == "") {
        args.push_back("-");
    } else {
        args.push_back(p_preset->get("codesign/identity"));
    }

    args.push_back("-v"); /* provide some more feedback */

    if (p_preset->get("codesign/replace_existing_signature")) {
        args.push_back("-f");
    }

    args.push_back(p_path);

    String str;
    Error err = OS::get_singleton()
                    ->execute("codesign", args, true, NULL, &str, NULL, true);
    ERR_FAIL_COND_V(err != OK, err);

    print_line("codesign (" + p_path + "):\n" + str);
    if (str.find("no identity found") != -1) {
        EditorNode::add_io_error("codesign: no identity found");
        return FAILED;
    }
    if ((str.find("unrecognized blob type") != -1)
        || (str.find("cannot read entitlement data") != -1)) {
        EditorNode::add_io_error("codesign: invalid entitlements file");
        return FAILED;
    }
#endif

    return OK;
}

Error MacOSEditorExportPlatform::_create_dmg(
    const String& p_dmg_path,
    const String& p_pkg_name,
    const String& p_app_path_name
) {
    List<String> args;

    if (FileAccess::exists(p_dmg_path)) {
        OS::get_singleton()->move_to_trash(p_dmg_path);
    }

    args.push_back("create");
    args.push_back(p_dmg_path);
    args.push_back("-volname");
    args.push_back(p_pkg_name);
    args.push_back("-fs");
    args.push_back("HFS+");
    args.push_back("-srcfolder");
    args.push_back(p_app_path_name);

    String str;
    Error err =
        OS::get_singleton()
            ->execute("hdiutil", args, true, nullptr, &str, nullptr, true);
    ERR_FAIL_COND_V(err != OK, err);

    print_line("hdiutil returned: " + str);
    if (str.find("create failed") != -1) {
        if (str.find("File exists") != -1) {
            EditorNode::add_io_error("hdiutil: create failed - file exists");
        } else {
            EditorNode::add_io_error("hdiutil: create failed");
        }
        return FAILED;
    }

    return OK;
}

Error MacOSEditorExportPlatform::export_project(
    const Ref<EditorExportPreset>& p_preset,
    bool p_debug,
    const String& p_path,
    int p_flags
) {
    ExportNotifier notifier(*this, p_preset, p_debug, p_path, p_flags);

    String src_pkg_name;

    EditorProgress ep("export", "Exporting for MacOS", 3, true);

    if (p_debug) {
        src_pkg_name = p_preset->get("custom_template/debug");
    } else {
        src_pkg_name = p_preset->get("custom_template/release");
    }

    if (src_pkg_name == "") {
        String err;
        src_pkg_name = find_export_template("macos.zip", &err);
        if (src_pkg_name == "") {
            EditorNode::add_io_error(err);
            return ERR_FILE_NOT_FOUND;
        }
    }

    if (!DirAccess::exists(p_path.get_base_dir())) {
        return ERR_FILE_BAD_PATH;
    }

    FileAccess* src_f    = nullptr;
    zlib_filefunc_def io = zipio_create_io_from_file(&src_f);

    if (ep.step("Creating app", 0)) {
        return ERR_SKIP;
    }

    unzFile src_pkg_zip = unzOpen2(src_pkg_name.utf8().get_data(), &io);
    if (!src_pkg_zip) {
        EditorNode::add_io_error(
            "Could not find template app to export:\n" + src_pkg_name
        );
        return ERR_FILE_NOT_FOUND;
    }

    int ret = unzGoToFirstFile(src_pkg_zip);

    String binary_to_use =
        "rebel_macos_" + String(p_debug ? "debug" : "release") + ".64";

    String pkg_name;
    if (p_preset->get("application/name") != "") {
        pkg_name = p_preset->get("application/name"); // app_name
    } else if (String(ProjectSettings::get_singleton()->get("application/config/name")) != "") {
        pkg_name = String(
            ProjectSettings::get_singleton()->get("application/config/name")
        );
    } else {
        pkg_name = "Unnamed";
    }

    pkg_name = OS::get_singleton()->get_safe_dir_name(pkg_name);

    String export_format = use_dmg() && p_path.ends_with("dmg") ? "dmg" : "zip";

    // Create our application bundle.
    String tmp_app_dir_name = pkg_name + ".app";
    String tmp_app_path_name =
        EditorSettings::get_singleton()->get_cache_dir().plus_file(
            tmp_app_dir_name
        );
    print_line("Exporting to " + tmp_app_path_name);

    Error err = OK;

    DirAccessRef tmp_app_dir = DirAccess::create_for_path(tmp_app_path_name);
    if (!tmp_app_dir) {
        err = ERR_CANT_CREATE;
    }

    // Create our folder structure.
    if (err == OK) {
        print_line("Creating " + tmp_app_path_name + "/Contents/MacOS");
        err = tmp_app_dir->make_dir_recursive(
            tmp_app_path_name + "/Contents/MacOS"
        );
    }

    if (err == OK) {
        print_line("Creating " + tmp_app_path_name + "/Contents/Frameworks");
        err = tmp_app_dir->make_dir_recursive(
            tmp_app_path_name + "/Contents/Frameworks"
        );
    }

    if (err == OK) {
        print_line("Creating " + tmp_app_path_name + "/Contents/Resources");
        err = tmp_app_dir->make_dir_recursive(
            tmp_app_path_name + "/Contents/Resources"
        );
    }

    Vector<String> translations =
        ProjectSettings::get_singleton()->get("locale/translations");
    if (translations.size() > 0) {
        {
            String fname = tmp_app_path_name + "/Contents/Resources/en.lproj";
            tmp_app_dir->make_dir_recursive(fname);
            FileAccessRef f = FileAccess::open(
                fname + "/InfoPlist.strings",
                FileAccess::WRITE
            );
        }

        for (int i = 0; i < translations.size(); i++) {
            Ref<Translation> tr = ResourceLoader::load(translations[i]);
            if (tr.is_valid()) {
                String fname = tmp_app_path_name + "/Contents/Resources/"
                             + tr->get_locale() + ".lproj";
                tmp_app_dir->make_dir_recursive(fname);
                FileAccessRef f = FileAccess::open(
                    fname + "/InfoPlist.strings",
                    FileAccess::WRITE
                );
            }
        }
    }

    // Now process our template.
    bool found_binary = false;
    Vector<String> dylibs_found;

    while (ret == UNZ_OK && err == OK) {
        bool is_execute = false;

        // Get filename.
        unz_file_info info;
        char fname[16384];
        ret = unzGetCurrentFileInfo(
            src_pkg_zip,
            &info,
            fname,
            16384,
            nullptr,
            0,
            nullptr,
            0
        );

        String file = String::utf8(fname);

        Vector<uint8_t> data;
        data.resize(info.uncompressed_size);

        // Read.
        unzOpenCurrentFile(src_pkg_zip);
        unzReadCurrentFile(src_pkg_zip, data.ptrw(), data.size());
        unzCloseCurrentFile(src_pkg_zip);

        // Write.
        file = file.replace_first("macos_template.app/", "");

        if (file == "Contents/Info.plist") {
            _fix_plist(p_preset, data, pkg_name);
        }

        if (file.begins_with("Contents/MacOS/rebel_")) {
            if (file != "Contents/MacOS/" + binary_to_use) {
                ret = unzGoToNextFile(src_pkg_zip);
                continue; // skip
            }
            found_binary = true;
            is_execute   = true;
            file         = "Contents/MacOS/" + pkg_name;
        }

        if (file == "Contents/Resources/icon.icns") {
            // See if there is an icon.
            String iconpath;
            if (p_preset->get("application/icon") != "") {
                iconpath = p_preset->get("application/icon");
            } else {
                iconpath = ProjectSettings::get_singleton()->get(
                    "application/config/icon"
                );
            }

            if (iconpath != "") {
                if (iconpath.get_extension() == "icns") {
                    FileAccess* icon =
                        FileAccess::open(iconpath, FileAccess::READ);
                    if (icon) {
                        data.resize(icon->get_len());
                        icon->get_buffer(&data.write[0], icon->get_len());
                        icon->close();
                        memdelete(icon);
                    }
                } else {
                    Ref<Image> icon;
                    icon.instance();
                    icon->load(iconpath);
                    if (!icon->empty()) {
                        _make_icon(icon, data);
                    }
                }
            }
        }

        if (data.size() > 0) {
            if (file.find("/data.mono.macos.64.release_debug/") != -1) {
                if (!p_debug) {
                    ret = unzGoToNextFile(src_pkg_zip);
                    continue; // skip
                }
                file = file.replace(
                    "/data.mono.macos.64.release_debug/",
                    "/GodotSharp/"
                );
            }
            if (file.find("/data.mono.macos.64.release/") != -1) {
                if (p_debug) {
                    ret = unzGoToNextFile(src_pkg_zip);
                    continue; // skip
                }
                file = file.replace(
                    "/data.mono.macos.64.release/",
                    "/GodotSharp/"
                );
            }

            if (file.ends_with(".dylib")) {
                dylibs_found.push_back(file);
            }

            print_line("ADDING: " + file + " size: " + itos(data.size()));

            // Write it into our application bundle.
            file = tmp_app_path_name.plus_file(file);
            if (err == OK) {
                err = tmp_app_dir->make_dir_recursive(file.get_base_dir());
            }
            if (err == OK) {
                FileAccess* f = FileAccess::open(file, FileAccess::WRITE);
                if (f) {
                    f->store_buffer(data.ptr(), data.size());
                    f->close();
                    if (is_execute) {
                        // chmod with 0755 if the file is executable.
                        FileAccess::set_unix_permissions(file, 0755);
                    }
                    memdelete(f);
                } else {
                    err = ERR_CANT_CREATE;
                }
            }
        }

        ret = unzGoToNextFile(src_pkg_zip);
    }

    // We're done with our source zip.
    unzClose(src_pkg_zip);

    if (!found_binary) {
        ERR_PRINT(
            "Requested template binary '" + binary_to_use
            + "' not found. It might be missing from your template archive."
        );
        err = ERR_FILE_NOT_FOUND;
    }

    if (err == OK) {
        if (ep.step("Making PKG", 1)) {
            return ERR_SKIP;
        }

        String pack_path =
            tmp_app_path_name + "/Contents/Resources/" + pkg_name + ".pck";
        Vector<SharedObject> shared_objects;
        err = save_pack(p_preset, pack_path, &shared_objects);

        // See if we can code sign our new package.
        bool sign_enabled = p_preset->get("codesign/enable");

        String ent_path = p_preset->get("codesign/entitlements/custom_file");
        if (sign_enabled && (ent_path == "")) {
            ent_path =
                EditorSettings::get_singleton()->get_cache_dir().plus_file(
                    pkg_name + ".entitlements"
                );

            FileAccess* ent_f = FileAccess::open(ent_path, FileAccess::WRITE);
            if (ent_f) {
                ent_f->store_line("<?xml version=\"1.0\" encoding=\"UTF-8\"?>");
                ent_f->store_line(
                    "<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" "
                    "\"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">"
                );
                ent_f->store_line("<plist version=\"1.0\">");
                ent_f->store_line("<dict>");
                if (Engine::get_singleton()->has_singleton("GodotSharp")) {
                    // These entitlements are required to run managed code, and
                    // are always enabled in Mono builds.
                    ent_f->store_line(
                        "<key>com.apple.security.cs.allow-jit</key>"
                    );
                    ent_f->store_line("<true/>");
                    ent_f->store_line(
                        "<key>com.apple.security.cs.allow-unsigned-executable-"
                        "memory</key>"
                    );
                    ent_f->store_line("<true/>");
                    ent_f->store_line(
                        "<key>com.apple.security.cs.allow-dyld-environment-"
                        "variables</key>"
                    );
                    ent_f->store_line("<true/>");
                } else {
                    if ((bool)p_preset->get(
                            "codesign/entitlements/allow_jit_code_execution"
                        )) {
                        ent_f->store_line(
                            "<key>com.apple.security.cs.allow-jit</key>"
                        );
                        ent_f->store_line("<true/>");
                    }
                    if ((bool)p_preset->get("codesign/entitlements/"
                                            "allow_unsigned_executable_memory"
                        )) {
                        ent_f->store_line(
                            "<key>com.apple.security.cs.allow-unsigned-"
                            "executable-memory</key>"
                        );
                        ent_f->store_line("<true/>");
                    }
                    if ((bool)p_preset->get("codesign/entitlements/"
                                            "allow_dyld_environment_variables"
                        )) {
                        ent_f->store_line(
                            "<key>com.apple.security.cs.allow-dyld-environment-"
                            "variables</key>"
                        );
                        ent_f->store_line("<true/>");
                    }
                }

                if ((bool)p_preset->get(
                        "codesign/entitlements/disable_library_validation"
                    )) {
                    ent_f->store_line(
                        "<key>com.apple.security.cs.disable-library-validation<"
                        "/key>"
                    );
                    ent_f->store_line("<true/>");
                }
                if ((bool)p_preset->get("codesign/entitlements/audio_input")) {
                    ent_f->store_line(
                        "<key>com.apple.security.device.audio-input</key>"
                    );
                    ent_f->store_line("<true/>");
                }
                if ((bool)p_preset->get("codesign/entitlements/camera")) {
                    ent_f->store_line(
                        "<key>com.apple.security.device.camera</key>"
                    );
                    ent_f->store_line("<true/>");
                }
                if ((bool)p_preset->get("codesign/entitlements/location")) {
                    ent_f->store_line(
                        "<key>com.apple.security.personal-information.location<"
                        "/key>"
                    );
                    ent_f->store_line("<true/>");
                }
                if ((bool)p_preset->get("codesign/entitlements/address_book")) {
                    ent_f->store_line(
                        "<key>com.apple.security.personal-information."
                        "addressbook</key>"
                    );
                    ent_f->store_line("<true/>");
                }
                if ((bool)p_preset->get("codesign/entitlements/calendars")) {
                    ent_f->store_line(
                        "<key>com.apple.security.personal-information."
                        "calendars</key>"
                    );
                    ent_f->store_line("<true/>");
                }
                if ((bool)p_preset->get("codesign/entitlements/photos_library"
                    )) {
                    ent_f->store_line(
                        "<key>com.apple.security.personal-information.photos-"
                        "library</key>"
                    );
                    ent_f->store_line("<true/>");
                }
                if ((bool)p_preset->get("codesign/entitlements/apple_events")) {
                    ent_f->store_line(
                        "<key>com.apple.security.automation.apple-events</key>"
                    );
                    ent_f->store_line("<true/>");
                }
                if ((bool)p_preset->get("codesign/entitlements/debugging")) {
                    ent_f->store_line(
                        "<key>com.apple.security.get-task-allow</key>"
                    );
                    ent_f->store_line("<true/>");
                }

                if ((bool
                    )p_preset->get("codesign/entitlements/app_sandbox/enabled"
                    )) {
                    ent_f->store_line(
                        "<key>com.apple.security.app-sandbox</key>"
                    );
                    ent_f->store_line("<true/>");

                    if ((bool)p_preset->get(
                            "codesign/entitlements/app_sandbox/network_server"
                        )) {
                        ent_f->store_line(
                            "<key>com.apple.security.network.server</key>"
                        );
                        ent_f->store_line("<true/>");
                    }
                    if ((bool)p_preset->get(
                            "codesign/entitlements/app_sandbox/network_client"
                        )) {
                        ent_f->store_line(
                            "<key>com.apple.security.network.client</key>"
                        );
                        ent_f->store_line("<true/>");
                    }
                    if ((bool)p_preset->get(
                            "codesign/entitlements/app_sandbox/device_usb"
                        )) {
                        ent_f->store_line(
                            "<key>com.apple.security.device.usb</key>"
                        );
                        ent_f->store_line("<true/>");
                    }
                    if ((bool)p_preset->get(
                            "codesign/entitlements/app_sandbox/device_bluetooth"
                        )) {
                        ent_f->store_line(
                            "<key>com.apple.security.device.bluetooth</key>"
                        );
                        ent_f->store_line("<true/>");
                    }
                    if ((int)p_preset->get(
                            "codesign/entitlements/app_sandbox/files_downloads"
                        )
                        == 1) {
                        ent_f->store_line(
                            "<key>com.apple.security.files.downloads.read-only<"
                            "/key>"
                        );
                        ent_f->store_line("<true/>");
                    }
                    if ((int)p_preset->get(
                            "codesign/entitlements/app_sandbox/files_downloads"
                        )
                        == 2) {
                        ent_f->store_line(
                            "<key>com.apple.security.files.downloads.read-"
                            "write</key>"
                        );
                        ent_f->store_line("<true/>");
                    }
                    if ((int)p_preset->get(
                            "codesign/entitlements/app_sandbox/files_pictures"
                        )
                        == 1) {
                        ent_f->store_line(
                            "<key>com.apple.security.files.pictures.read-only</"
                            "key>"
                        );
                        ent_f->store_line("<true/>");
                    }
                    if ((int)p_preset->get(
                            "codesign/entitlements/app_sandbox/files_pictures"
                        )
                        == 2) {
                        ent_f->store_line(
                            "<key>com.apple.security.files.pictures.read-write<"
                            "/key>"
                        );
                        ent_f->store_line("<true/>");
                    }
                    if ((int)p_preset->get(
                            "codesign/entitlements/app_sandbox/files_music"
                        )
                        == 1) {
                        ent_f->store_line(
                            "<key>com.apple.security.files.music.read-only</"
                            "key>"
                        );
                        ent_f->store_line("<true/>");
                    }
                    if ((int)p_preset->get(
                            "codesign/entitlements/app_sandbox/files_music"
                        )
                        == 2) {
                        ent_f->store_line(
                            "<key>com.apple.security.files.music.read-write</"
                            "key>"
                        );
                        ent_f->store_line("<true/>");
                    }
                    if ((int)p_preset->get(
                            "codesign/entitlements/app_sandbox/files_movies"
                        )
                        == 1) {
                        ent_f->store_line(
                            "<key>com.apple.security.files.movies.read-only</"
                            "key>"
                        );
                        ent_f->store_line("<true/>");
                    }
                    if ((int)p_preset->get(
                            "codesign/entitlements/app_sandbox/files_movies"
                        )
                        == 2) {
                        ent_f->store_line(
                            "<key>com.apple.security.files.movies.read-write</"
                            "key>"
                        );
                        ent_f->store_line("<true/>");
                    }
                }

                ent_f->store_line("</dict>");
                ent_f->store_line("</plist>");

                ent_f->close();
                memdelete(ent_f);
            } else {
                err = ERR_CANT_CREATE;
            }
        }

        if (err == OK) {
            DirAccess* da = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
            for (int i = 0; i < shared_objects.size(); i++) {
                String src_path =
                    ProjectSettings::get_singleton()->globalize_path(
                        shared_objects[i].path
                    );
                if (da->dir_exists(src_path)) {
#ifndef UNIX_ENABLED
                    WARN_PRINT(
                        "Relative symlinks are not supported, exported "
                        + src_path.get_file() + " might be broken!"
                    );
#endif
                    print_verbose(
                        "export framework: " + src_path + " -> "
                        + tmp_app_path_name + "/Contents/Frameworks/"
                        + src_path.get_file()
                    );
                    err = da->make_dir_recursive(
                        tmp_app_path_name + "/Contents/Frameworks/"
                        + src_path.get_file()
                    );
                    if (err == OK) {
                        err = da->copy_dir(
                            src_path,
                            tmp_app_path_name + "/Contents/Frameworks/"
                                + src_path.get_file(),
                            -1,
                            true
                        );
                    }
                } else {
                    print_verbose(
                        "export dylib: " + src_path + " -> " + tmp_app_path_name
                        + "/Contents/Frameworks/" + src_path.get_file()
                    );
                    err = da->copy(
                        src_path,
                        tmp_app_path_name + "/Contents/Frameworks/"
                            + src_path.get_file()
                    );
                }
                if (err == OK && sign_enabled) {
                    err = _code_sign(
                        p_preset,
                        tmp_app_path_name + "/Contents/Frameworks/"
                            + src_path.get_file(),
                        ent_path
                    );
                }
            }
            memdelete(da);
        }

        if (sign_enabled) {
            for (int i = 0; i < dylibs_found.size(); i++) {
                if (err == OK) {
                    err = _code_sign(
                        p_preset,
                        tmp_app_path_name + "/" + dylibs_found[i],
                        ent_path
                    );
                }
            }
        }

        if (err == OK && sign_enabled) {
            if (ep.step("Code signing bundle", 2)) {
                return ERR_SKIP;
            }
            err = _code_sign(
                p_preset,
                tmp_app_path_name + "/Contents/MacOS/" + pkg_name,
                ent_path
            );
        }

        if (export_format == "dmg") {
            // Create a DMG.
            if (err == OK) {
                if (ep.step("Making DMG", 3)) {
                    return ERR_SKIP;
                }
                err = _create_dmg(p_path, pkg_name, tmp_app_path_name);
            }
            // Sign DMG.
            if (err == OK && sign_enabled) {
                if (ep.step("Code signing DMG", 3)) {
                    return ERR_SKIP;
                }
                err = _code_sign(p_preset, p_path, ent_path);
            }
        } else {
            // Create ZIP.
            if (err == OK) {
                if (ep.step("Making ZIP", 3)) {
                    return ERR_SKIP;
                }
                if (FileAccess::exists(p_path)) {
                    OS::get_singleton()->move_to_trash(p_path);
                }

                FileAccess* dst_f        = nullptr;
                zlib_filefunc_def io_dst = zipio_create_io_from_file(&dst_f);
                zipFile zip              = zipOpen2(
                    p_path.utf8().get_data(),
                    APPEND_STATUS_CREATE,
                    nullptr,
                    &io_dst
                );

                _zip_folder_recursive(
                    zip,
                    EditorSettings::get_singleton()->get_cache_dir(),
                    pkg_name + ".app",
                    pkg_name
                );

                zipClose(zip, nullptr);
            }
        }

        bool noto_enabled = p_preset->get("notarization/enable");
        if (err == OK && noto_enabled) {
            if (ep.step("Sending archive for notarization", 4)) {
                return ERR_SKIP;
            }
            err = _notarize(p_preset, p_path);
        }

        // Clean up temporary .app dir.
        tmp_app_dir->change_dir(tmp_app_path_name);
        tmp_app_dir->erase_contents_recursive();
        tmp_app_dir->change_dir("..");
        tmp_app_dir->remove(tmp_app_dir_name);
    }

    return err;
}

void MacOSEditorExportPlatform::_zip_folder_recursive(
    zipFile& p_zip,
    const String& p_root_path,
    const String& p_folder,
    const String& p_pkg_name
) {
    String dir = p_root_path.plus_file(p_folder);

    DirAccess* da = DirAccess::open(dir);
    da->list_dir_begin();
    String f;
    while ((f = da->get_next()) != "") {
        if (f == "." || f == "..") {
            continue;
        }
        if (da->is_link(f)) {
            OS::Time time = OS::get_singleton()->get_time();
            OS::Date date = OS::get_singleton()->get_date();

            zip_fileinfo zipfi;
            zipfi.tmz_date.tm_hour = time.hour;
            zipfi.tmz_date.tm_mday = date.day;
            zipfi.tmz_date.tm_min  = time.min;
            // ctime's tm time structure's month range is 0-11
            // We use a month range of 1-12
            // Ref: http://www.cplusplus.com/reference/ctime/tm/
            zipfi.tmz_date.tm_mon  = date.month - 1;
            zipfi.tmz_date.tm_sec  = time.sec;
            zipfi.tmz_date.tm_year = date.year;
            zipfi.dosDate          = 0;
            // 0120000: symbolic link type
            // 0000755: permissions rwxr-xr-x
            // 0000644: permissions rw-r--r--
            uint32_t _mode         = 0120644;
            zipfi.external_fa      = (_mode << 16L) | !(_mode & 0200);
            zipfi.internal_fa      = 0;

            zipOpenNewFileInZip4(
                p_zip,
                p_folder.plus_file(f).utf8().get_data(),
                &zipfi,
                nullptr,
                0,
                nullptr,
                0,
                nullptr,
                Z_DEFLATED,
                Z_DEFAULT_COMPRESSION,
                0,
                -MAX_WBITS,
                DEF_MEM_LEVEL,
                Z_DEFAULT_STRATEGY,
                nullptr,
                0,
                0x0314, // "version made by", 0x03 - Unix, 0x14 - ZIP
                        // specification version 2.0, required to store Unix
                        // file permissions
                0
            );

            String target = da->read_link(f);
            zipWriteInFileInZip(
                p_zip,
                target.utf8().get_data(),
                target.utf8().size()
            );
            zipCloseFileInZip(p_zip);
        } else if (da->current_is_dir()) {
            _zip_folder_recursive(
                p_zip,
                p_root_path,
                p_folder.plus_file(f),
                p_pkg_name
            );
        } else {
            bool is_executable =
                (p_folder.ends_with("MacOS") && (f == p_pkg_name));

            OS::Time time = OS::get_singleton()->get_time();
            OS::Date date = OS::get_singleton()->get_date();

            zip_fileinfo zipfi;
            zipfi.tmz_date.tm_hour = time.hour;
            zipfi.tmz_date.tm_mday = date.day;
            zipfi.tmz_date.tm_min  = time.min;
            // ctime's tm time structure's month range is 0-11
            // We use a month range of 1-12
            // Ref: http://www.cplusplus.com/reference/ctime/tm/
            zipfi.tmz_date.tm_mon  = date.month - 1;
            zipfi.tmz_date.tm_sec  = time.sec;
            zipfi.tmz_date.tm_year = date.year;
            zipfi.dosDate          = 0;
            // 0100000: regular file type
            // 0000755: permissions rwxr-xr-x
            // 0000644: permissions rw-r--r--
            uint32_t _mode         = (is_executable ? 0100755 : 0100644);
            zipfi.external_fa      = (_mode << 16L) | !(_mode & 0200);
            zipfi.internal_fa      = 0;

            zipOpenNewFileInZip4(
                p_zip,
                p_folder.plus_file(f).utf8().get_data(),
                &zipfi,
                nullptr,
                0,
                nullptr,
                0,
                nullptr,
                Z_DEFLATED,
                Z_DEFAULT_COMPRESSION,
                0,
                -MAX_WBITS,
                DEF_MEM_LEVEL,
                Z_DEFAULT_STRATEGY,
                nullptr,
                0,
                0x0314, // "version made by", 0x03 - Unix, 0x14 - ZIP
                        // specification version 2.0, required to store Unix
                        // file permissions
                0
            );

            FileAccessRef fa =
                FileAccess::open(dir.plus_file(f), FileAccess::READ);
            if (!fa) {
                ERR_FAIL_MSG(
                    "Can't open file to read from path '"
                    + String(dir.plus_file(f)) + "'."
                );
            }
            const int bufsize = 16384;
            uint8_t buf[bufsize];

            while (true) {
                uint64_t got = fa->get_buffer(buf, bufsize);
                if (got == 0) {
                    break;
                }
                zipWriteInFileInZip(p_zip, buf, got);
            }

            zipCloseFileInZip(p_zip);
        }
    }
    da->list_dir_end();
    memdelete(da);
}

bool MacOSEditorExportPlatform::can_export(
    const Ref<EditorExportPreset>& p_preset,
    String& r_error,
    bool& r_missing_templates
) const {
    String err;
    bool valid = false;

    // Look for export templates (first official, and if defined custom
    // templates).

    bool dvalid = exists_export_template("macos.zip", &err);
    bool rvalid = dvalid; // Both in the same ZIP.

    if (p_preset->get("custom_template/debug") != "") {
        dvalid = FileAccess::exists(p_preset->get("custom_template/debug"));
        if (!dvalid) {
            err += TTR("Custom debug template not found.") + "\n";
        }
    }
    if (p_preset->get("custom_template/release") != "") {
        rvalid = FileAccess::exists(p_preset->get("custom_template/release"));
        if (!rvalid) {
            err += TTR("Custom release template not found.") + "\n";
        }
    }

    valid               = dvalid || rvalid;
    r_missing_templates = !valid;

    String identifier = p_preset->get("application/identifier");
    String pn_err;
    if (!is_package_name_valid(identifier, &pn_err)) {
        err   += TTR("Invalid bundle identifier:") + " " + pn_err + "\n";
        valid  = false;
    }

    bool sign_enabled = p_preset->get("codesign/enable");
    bool noto_enabled = p_preset->get("notarization/enable");
    if (noto_enabled) {
        if (!sign_enabled) {
            err   += TTR("Notarization: code signing required.") + "\n";
            valid  = false;
        }
        bool hr_enabled = p_preset->get("codesign/hardened_runtime");
        if (!hr_enabled) {
            err   += TTR("Notarization: hardened runtime required.") + "\n";
            valid  = false;
        }
        if (p_preset->get("notarization/apple_id_name") == "") {
            err   += TTR("Notarization: Apple ID name not specified.") + "\n";
            valid  = false;
        }
        if (p_preset->get("notarization/apple_id_password") == "") {
            err += TTR("Notarization: Apple ID password not specified.") + "\n";
            valid = false;
        }
    }

    if (!err.empty()) {
        r_error = err;
    }
    return valid;
}

MacOSEditorExportPlatform::MacOSEditorExportPlatform() {
    Ref<Image> img = memnew(Image(_macos_logo));
    logo.instance();
    logo->create_from_image(img);
}

MacOSEditorExportPlatform::~MacOSEditorExportPlatform() {}

void register_macos_exporter() {
    Ref<MacOSEditorExportPlatform> platform;
    platform.instance();

    EditorExport::get_singleton()->add_export_platform(platform);
}
