#!/usr/bin/env python

# Rebel SCons build file
#
# 1. Identify and check the target platform
# 2. Create the SCons build Environment
# 3. Define the build Options
# 3.1 Target build options
# 3.2 Component options
# 3.3 Advanced options
# 3.4 Thirdparty libraries to include
# 3.5 Compiler and linker options
# 3.6 Platform-specific options
# 4. Identify and add modules
# 4.1 Add module options

EnsureSConsVersion(4, 1)

# System imports
import atexit
import glob
import os
import sys
import time
from collections import OrderedDict

# Rebel build scripts
import methods
import tools.scripts.build as build

time_at_start = time.time()

# 1. Identify and check the target platform
###########################################

selected_platform = ARGUMENTS.get("platform", ARGUMENTS.get("p", ""))

if selected_platform == "list":
    build.print_supported_platforms()
    sys.exit(0)

if selected_platform == "":
    selected_platform = build.detect_platform()

platform = build.get_platform(selected_platform)

# Create platforms icon header files
build.create_platforms_icon_headers()

# 2. Create the SCons build Environment
#######################################

use_mingw = ARGUMENTS.get("use_mingw", False)
custom_tools = ["default"]
if selected_platform == "android":
    custom_tools = ["clang", "clang++", "as", "ar", "link"]
elif selected_platform == "web":
    # Use generic POSIX build toolchain for Emscripten.
    custom_tools = ["cc", "c++", "ar", "link", "textfile", "zip"]
elif use_mingw or (os.name == "posix" and selected_platform == "windows"):
    custom_tools = ["mingw"]

environment = Environment(tools=custom_tools)

# SCons does not use the external environmental variables.
# We need to manually specify which external environment variables to keep.
if "PATH" in os.environ:
    environment["ENV"]["PATH"] = os.environ["PATH"]
if "PKG_CONFIG_PATH" in os.environ:
    environment["ENV"]["PKG_CONFIG_PATH"] = os.environ["PKG_CONFIG_PATH"]
if "TERM" in os.environ:
    environment["ENV"]["TERM"] = os.environ["TERM"]

environment.disabled_modules = []
environment.use_ptrcall = False
environment.module_version_string = ""
environment.msvc = False

environment.__class__.disable_module = methods.disable_module
environment.__class__.add_module_version_string = methods.add_module_version_string
environment.__class__.add_source_files = methods.add_source_files
environment.__class__.use_windows_spawn_fix = methods.use_windows_spawn_fix
environment.__class__.split_lib = methods.split_lib
environment.__class__.add_shared_library = methods.add_shared_library
environment.__class__.add_library = methods.add_library
environment.__class__.add_program = methods.add_program
environment.__class__.CommandNoCache = methods.CommandNoCache
environment.__class__.disable_warnings = methods.disable_warnings

environment["x86_libtheora_opt_gcc"] = False
environment["x86_libtheora_opt_vc"] = False

sys.exit(0)

# 3. Define the build Options
#############################

custom_options_files = ["custom.py"]
profile = ARGUMENTS.get("profile", "")
if profile:
    if os.path.isfile(profile):
        custom_options_files.append(profile)
    elif os.path.isfile(profile + ".py"):
        custom_options_files.append(profile + ".py")

options = Variables(custom_options_files, ARGUMENTS)

# 3.1 Target build options
options.Add(
    "platform", "Target platform (use platform=list to list supported platforms)", ""
)
options.Add("p", "Platform (alias for 'platform')", "")
options.Add(BoolVariable("tools", "Build the tools (a.k.a. the Rebel editor)", True))
options.Add(
    EnumVariable(
        "target", "Compilation target", "debug", ("debug", "release_debug", "release")
    )
)
options.Add("arch", "Platform-dependent architecture (arm/arm64/x86/x64/mips/...)", "")
options.Add(
    EnumVariable("bits", "Target platform bits", "default", ("default", "32", "64"))
)
options.Add(
    EnumVariable("optimize", "Optimization type", "speed", ("speed", "size", "none"))
)
options.Add(
    BoolVariable(
        "production", "Set defaults to build Rebel for use in production", False
    )
)
options.Add(BoolVariable("use_lto", "Use link-time optimization", False))

# 3.2 Component options
options.Add(BoolVariable("deprecated", "Enable deprecated features", True))
options.Add(BoolVariable("minizip", "Enable ZIP archive support using minizip", True))
options.Add(BoolVariable("xaudio2", "Enable the XAudio2 audio driver", False))
options.Add(
    "custom_modules",
    "A list of comma-separated directory paths containing custom modules to build",
    "",
)
options.Add(
    BoolVariable(
        "custom_modules_recursive",
        "Detect custom modules recursively for each path specified in 'custom_modules'",
        True,
    )
)

# 3.3 Advanced options
options.Add(
    BoolVariable("dev", "Alias for verbose=yes warnings=extra werror=yes", False)
)
options.Add(
    BoolVariable("fast_unsafe", "Enable unsafe options for faster rebuilds", False)
)
options.Add(BoolVariable("verbose", "Enable verbose output for the compilation", False))
options.Add(
    BoolVariable("progress", "Show a progress indicator during compilation", True)
)
options.Add(
    EnumVariable(
        "warnings",
        "Level of compilation warnings",
        "all",
        ("extra", "all", "moderate", "no"),
    )
)
options.Add(BoolVariable("werror", "Treat compiler warnings as errors", False))
options.Add(
    "extra_suffix",
    "Custom extra suffix added to the base filename of all generated binary files",
    "",
)
options.Add(BoolVariable("vsproj", "Generate a Visual Studio solution", False))
options.Add(
    BoolVariable(
        "split_libmodules",
        "Split intermediate libmodules.a in smaller chunks to prevent exceeding linker command line size (forced to True when using MinGW)",
        False,
    )
)
options.Add(
    BoolVariable("disable_3d", "Disable 3D nodes for a smaller executable", False)
)
options.Add(
    BoolVariable(
        "disable_advanced_gui", "Disable advanced GUI nodes and behaviors", False
    )
)
options.Add(
    BoolVariable(
        "no_editor_splash", "Don't use the custom splash screen for the editor", True
    )
)
options.Add(
    "system_certs_path",
    "Use this path as SSL certificates default for editor (for package maintainers)",
    "",
)
options.Add(
    BoolVariable(
        "use_precise_math_checks",
        "Math checks use very precise epsilon (debug option)",
        False,
    )
)

# 3.4 Thirdparty libraries to include
options.Add(BoolVariable("builtin_bullet", "Use the built-in Bullet library", True))
options.Add(
    BoolVariable("builtin_certs", "Use the built-in SSL certificates bundles", True)
)
options.Add(BoolVariable("builtin_embree", "Use the built-in Embree library", True))
options.Add(BoolVariable("builtin_enet", "Use the built-in ENet library", True))
options.Add(BoolVariable("builtin_freetype", "Use the built-in FreeType library", True))
options.Add(BoolVariable("builtin_libogg", "Use the built-in libogg library", True))
options.Add(BoolVariable("builtin_libpng", "Use the built-in libpng library", True))
options.Add(
    BoolVariable("builtin_libtheora", "Use the built-in libtheora library", True)
)
options.Add(
    BoolVariable("builtin_libvorbis", "Use the built-in libvorbis library", True)
)
options.Add(BoolVariable("builtin_libvpx", "Use the built-in libvpx library", True))
options.Add(BoolVariable("builtin_libwebp", "Use the built-in libwebp library", True))
options.Add(BoolVariable("builtin_wslay", "Use the built-in wslay library", True))
options.Add(BoolVariable("builtin_mbedtls", "Use the built-in mbedTLS library", True))
options.Add(
    BoolVariable("builtin_miniupnpc", "Use the built-in miniupnpc library", True)
)
options.Add(BoolVariable("builtin_opus", "Use the built-in Opus library", True))
options.Add(BoolVariable("builtin_pcre2", "Use the built-in PCRE2 library", True))
options.Add(
    BoolVariable(
        "builtin_pcre2_with_jit",
        "Use JIT compiler for the built-in PCRE2 library",
        True,
    )
)
options.Add(BoolVariable("builtin_recast", "Use the built-in Recast library", True))
options.Add(BoolVariable("builtin_squish", "Use the built-in squish library", True))
options.Add(BoolVariable("builtin_xatlas", "Use the built-in xatlas library", True))
options.Add(BoolVariable("builtin_zlib", "Use the built-in zlib library", True))
options.Add(BoolVariable("builtin_zstd", "Use the built-in Zstd library", True))

# 3.5 Compiler and linker options
options.Add("CXX", "C++ compiler")
options.Add("CC", "C compiler")
options.Add("LINK", "Linker")
options.Add("CCFLAGS", "Custom flags for both the C and C++ compilers")
options.Add("CFLAGS", "Custom flags for the C compiler")
options.Add("CXXFLAGS", "Custom flags for the C++ compiler")
options.Add("LINKFLAGS", "Custom flags for the linker")

# 3.6 Platform-specific options
for option in platform.get_options():
    options.Add(option)

# 4. Identify and add modules
#############################

modules_detected = OrderedDict()
module_search_paths = ["modules"]  # Built-in path.

if environment["custom_modules"]:
    paths = environment["custom_modules"].split(",")
    for path in paths:
        try:
            module_search_paths.append(methods.convert_custom_modules_path(path))
        except ValueError as error:
            print(error)
            sys.exit(255)

for path in module_search_paths:
    if path == "modules":
        # Built-in modules don't have nested modules,
        # so save the time it takes to parse directories.
        modules = methods.detect_modules(path, recursive=False)
    else:
        # Custom.
        modules = methods.detect_modules(path, environment["custom_modules_recursive"])
        # Provide default include path for both the custom module search `path`
        # and the base directory containing custom modules, as it may be different
        # from the built-in "modules" name (e.g. "custom_modules/summator/summator.h"),
        # so it can be referenced simply as `#include "summator/summator.h"`
        # independently of where a module is located on user's filesystem.
        environment.Prepend(CPPPATH=[path, os.path.dirname(path)])
    # Note: custom modules can override built-in ones.
    modules_detected.update(modules)

# 4.1 Add module options
for name, path in modules_detected.items():
    enabled = True
    sys.path.insert(0, path)
    import config

    try:
        enabled = config.is_enabled()
    except AttributeError:
        pass
    sys.path.remove(path)
    sys.modules.pop("config")
    options.Add(
        BoolVariable(
            "module_" + name + "_enabled", "Enable module '%s'" % (name,), enabled
        )
    )

methods.write_modules(modules_detected)

# Update the environment with all the options.
options.Update(environment)
Help(options.GenerateHelpText(environment))

# Add default include paths.
environment.Prepend(CPPPATH=["#"])

# Build type defines - more platform-specific ones can be in detect.py.
if environment["target"] == "release_debug" or environment["target"] == "debug":
    # DEBUG_ENABLED enables debugging *features* and debug-only code, which is intended
    # to give *users* extra debugging information for their game development.
    environment.Append(CPPDEFINES=["DEBUG_ENABLED"])

if environment["target"] == "debug":
    # DEV_ENABLED enables *engine developer* code which should only be compiled for those
    # working on the engine itself.
    environment.Append(CPPDEFINES=["DEV_ENABLED"])

# SCons speed optimization controlled by the `fast_unsafe` option, which provide
# more than 10 s speed up for incremental rebuilds.
# Unsafe as they reduce the certainty of rebuilding all changed files, so it's
# enabled by default for `debug` builds, and can be overridden from command line.
# Ref: https://github.com/SCons/scons/wiki/GoFastButton
if methods.get_cmdline_bool("fast_unsafe", environment["target"] == "debug"):
    # Renamed to `content-timestamp` in SCons >= 4.2, keeping MD5 for compat.
    environment.Decider("MD5-timestamp")
    environment.SetOption("implicit_cache", 1)
    environment.SetOption("max_drift", 60)

if environment["use_precise_math_checks"]:
    environment.Append(CPPDEFINES=["PRECISE_MATH_CHECKS"])

if not environment.File("#main/splash_editor.png").exists():
    # Force disabling editor splash if missing.
    environment["no_editor_splash"] = True
if environment["no_editor_splash"]:
    environment.Append(CPPDEFINES=["NO_EDITOR_SPLASH"])

if not environment["deprecated"]:
    environment.Append(CPPDEFINES=["DISABLE_DEPRECATED"])

# Generating the compilation DB (`compile_commands.json`) requires SCons 4.0.0 or later.
from SCons import __version__ as scons_raw_version

scons_ver = environment._get_major_minor_revision(scons_raw_version)

if scons_ver >= (4, 0, 0):
    environment.Tool("compilation_db")
    environment.Alias("compiledb", environment.CompilationDatabase())

# 'dev' and 'production' are aliases to set default options if they haven't been set
# manually by the user.
if environment["dev"]:
    environment["verbose"] = methods.get_cmdline_bool("verbose", True)
    environment["warnings"] = ARGUMENTS.get("warnings", "extra")
    environment["werror"] = methods.get_cmdline_bool("werror", True)
if environment["production"]:
    environment["use_lto"] = methods.get_cmdline_bool("use_lto", True)
    print("use_lto is: " + str(environment["use_lto"]))
    environment["debug_symbols"] = methods.get_cmdline_bool("debug_symbols", False)
    if not environment["tools"] and environment["target"] == "debug":
        print(
            "WARNING: Requested `production` build with `tools=no target=debug`, "
            "this will give you a full debug template (use `target=release_debug` "
            "for an optimized template with debug features)."
        )
    if environment.msvc:
        print(
            "WARNING: For `production` Windows builds, you should use MinGW with GCC "
            "or Clang instead of Visual Studio, as they can better optimize the "
            "GDScript VM in a very significant way. MSVC LTO also doesn't work "
            "reliably for our use case."
            "If you want to use MSVC nevertheless for production builds, set "
            "`debug_symbols=no use_lto=no` instead of the `production=yes` option."
        )
        Exit(255)

environment.extra_suffix = ""

if environment["extra_suffix"] != "":
    environment.extra_suffix += "." + environment["extra_suffix"]

# Environment flags
CCFLAGS = environment.get("CCFLAGS", "")
environment["CCFLAGS"] = ""
environment.Append(CCFLAGS=str(CCFLAGS).split())

CFLAGS = environment.get("CFLAGS", "")
environment["CFLAGS"] = ""
environment.Append(CFLAGS=str(CFLAGS).split())

CXXFLAGS = environment.get("CXXFLAGS", "")
environment["CXXFLAGS"] = ""
environment.Append(CXXFLAGS=str(CXXFLAGS).split())

LINKFLAGS = environment.get("LINKFLAGS", "")
environment["LINKFLAGS"] = ""
environment.Append(LINKFLAGS=str(LINKFLAGS).split())

# Platform specific flags
flag_list = platform.get_flags()
for f in flag_list:
    if not (f[0] in ARGUMENTS):  # allow command line to override platform flags
        environment[f[0]] = f[1]

# Must happen after the flags definition, so that they can be used by platform detect
detect.configure(env)

# Set our C and C++ standard requirements.
# Prepending to make it possible to override
# This needs to come after `configure`, otherwise we don't have environment.msvc.
if not environment.msvc:
    # Specifying GNU extensions support explicitly, which are supported by
    # both GCC and Clang. This mirrors GCC and Clang's current default
    # compile flags if no -std is specified.
    environment.Prepend(CFLAGS=["-std=gnu11"])
    environment.Prepend(CXXFLAGS=["-std=gnu++14"])
else:
    # MSVC doesn't have clear C standard support, /std only covers C++.
    # We apply it to CCFLAGS (both C and C++ code) in case it impacts C features.
    environment.Prepend(CCFLAGS=["/std:c++14"])

# Configure compiler warnings
if environment.msvc:  # MSVC
    # Truncations, narrowing conversions, signed/unsigned comparisons...
    disable_nonessential_warnings = [
        "/wd4267",
        "/wd4244",
        "/wd4305",
        "/wd4018",
        "/wd4800",
    ]
    if environment["warnings"] == "extra":
        environment.Append(CCFLAGS=["/Wall"])  # Implies /W4
    elif environment["warnings"] == "all":
        environment.Append(CCFLAGS=["/W3"] + disable_nonessential_warnings)
    elif environment["warnings"] == "moderate":
        environment.Append(CCFLAGS=["/W2"] + disable_nonessential_warnings)
    else:  # 'no'
        environment.Append(CCFLAGS=["/w"])
    # Set exception handling model to avoid warnings caused by Windows system headers.
    environment.Append(CCFLAGS=["/EHsc"])

    if environment["werror"]:
        environment.Append(CCFLAGS=["/WX"])
else:  # GCC, Clang
    version = methods.get_compiler_version(env) or [-1, -1]

    common_warnings = []

    if methods.using_gcc(env):
        common_warnings += ["-Wno-misleading-indentation"]
        if version[0] >= 7:
            common_warnings += ["-Wshadow-local"]
    elif methods.using_clang(env) or methods.using_emcc(env):
        # We often implement `operator<` for structs of pointers as a requirement
        # for putting them in `Set` or `Map`. We don't mind about unreliable ordering.
        common_warnings += ["-Wno-ordered-compare-function-pointers"]

    if environment["warnings"] == "extra":
        # Note: enable -Wimplicit-fallthrough for Clang (already part of -Wextra for GCC)
        # once we switch to C++11 or later (necessary for our FALLTHROUGH macro).
        environment.Append(
            CCFLAGS=["-Wall", "-Wextra", "-Wwrite-strings", "-Wno-unused-parameter"]
            + common_warnings
        )
        environment.Append(CXXFLAGS=["-Wctor-dtor-privacy", "-Wnon-virtual-dtor"])
        if methods.using_gcc(env):
            environment.Append(
                CCFLAGS=[
                    "-Walloc-zero",
                    "-Wduplicated-branches",
                    "-Wduplicated-cond",
                    "-Wstringop-overflow=4",
                    "-Wlogical-op",
                ]
            )
            environment.Append(CXXFLAGS=["-Wnoexcept", "-Wplacement-new=1"])
            if version[0] >= 9:
                environment.Append(CCFLAGS=["-Wattribute-alias=2"])
    elif environment["warnings"] == "all":
        environment.Append(CCFLAGS=["-Wall"] + common_warnings)
    elif environment["warnings"] == "moderate":
        environment.Append(CCFLAGS=["-Wall", "-Wno-unused"] + common_warnings)
    else:  # 'no'
        environment.Append(CCFLAGS=["-w"])

    if environment["werror"]:
        environment.Append(CCFLAGS=["-Werror"])
        if (
            methods.using_gcc(env) and version[0] >= 12
        ):  # False positives in our error macros, see GH-58747.
            environment.Append(CCFLAGS=["-Wno-error=return-type"])

if hasattr(detect, "get_program_suffix"):
    suffix = "." + detect.get_program_suffix()
else:
    suffix = "." + platform

if environment["target"] == "release":
    if environment["tools"]:
        print("Tools can only be built with targets 'debug' and 'release_debug'.")
        sys.exit(255)
    suffix += ".opt"
    environment.Append(CPPDEFINES=["NDEBUG"])

elif environment["target"] == "release_debug":
    if environment["tools"]:
        suffix += ".opt.tools"
    else:
        suffix += ".opt.debug"
else:
    if environment["tools"]:
        suffix += ".tools"
    else:
        suffix += ".debug"

if environment["arch"] != "":
    suffix += "." + environment["arch"]
elif environment["bits"] == "32":
    suffix += ".32"
elif environment["bits"] == "64":
    suffix += ".64"

suffix += environment.extra_suffix

environment.modules_path = OrderedDict()
environment.modules_classes_docs_path = {}
environment.module_icons_paths = []

for module_name, path in modules_detected.items():
    if not environment["module_" + module_name + "_enabled"]:
        continue
    sys.path.insert(0, path)
    environment.current_module = module_name
    import config

    # can_build changed number of arguments between 3.0 (1) and 3.1 (2),
    # so try both to preserve compatibility for 3.0 modules
    can_build = False
    try:
        can_build = config.can_build(env, platform)
    except TypeError:
        print(
            "Warning: module '%s' uses a deprecated `can_build` "
            "signature in its config.py file, it should be "
            "`can_build(env, platform)`." % x
        )
        can_build = config.can_build(platform)
    if can_build:
        config.configure(env)
        # Get list of documented classes (if present)
        if "get_classes" in dir(config):
            classes = config.get_classes()
            for class_name in classes:
                environment.modules_classes_docs_path[class_name] = path + "/docs"
        environment.module_icons_paths.append(path + "/" + "icons")
        environment.modules_path[module_name] = path

    sys.path.remove(path)
    sys.modules.pop("config")

methods.update_version(environment.module_version_string)

environment["PROGSUFFIX"] = (
    suffix + environment.module_version_string + environment["PROGSUFFIX"]
)
environment["OBJSUFFIX"] = suffix + environment["OBJSUFFIX"]
# (SH)LIBSUFFIX will be used for our own built libraries
# LIBSUFFIXES contains LIBSUFFIX and SHLIBSUFFIX by default,
# so we need to append the default suffixes to keep the ability
# to link against thirdparty libraries (.a, .so, .lib, etc.).
if os.name == "nt":
    # On Windows, only static libraries and import libraries can be
    # statically linked - both using .lib extension
    environment["LIBSUFFIXES"] += [environment["LIBSUFFIX"]]
else:
    environment["LIBSUFFIXES"] += [environment["LIBSUFFIX"], environment["SHLIBSUFFIX"]]
environment["LIBSUFFIX"] = suffix + environment["LIBSUFFIX"]
environment["SHLIBSUFFIX"] = suffix + environment["SHLIBSUFFIX"]

if environment.use_ptrcall:
    environment.Append(CPPDEFINES=["PTRCALL_ENABLED"])
if environment["tools"]:
    environment.Append(CPPDEFINES=["TOOLS_ENABLED"])
if environment["disable_3d"]:
    if environment["tools"]:
        print(
            "Build option 'disable_3d=yes' cannot be used with 'tools=yes' (editor), "
            "only with 'tools=no' (export template)."
        )
        sys.exit(255)
    else:
        environment.Append(CPPDEFINES=["_3D_DISABLED"])
if environment["disable_advanced_gui"]:
    if environment["tools"]:
        print(
            "Build option 'disable_advanced_gui=yes' cannot be used with 'tools=yes' (editor), "
            "only with 'tools=no' (export template)."
        )
        sys.exit(255)
    else:
        environment.Append(CPPDEFINES=["ADVANCED_GUI_DISABLED"])
if environment["minizip"]:
    environment.Append(CPPDEFINES=["MINIZIP_ENABLED"])

editor_modules = ["freetype"]
for x in editor_modules:
    if not environment["module_" + x + "_enabled"]:
        if environment["tools"]:
            print(
                "Build option 'module_"
                + x
                + "_enabled=no' cannot be used with 'tools=yes' (editor), "
                "only with 'tools=no' (export template)."
            )
            sys.exit(255)

if not environment["verbose"]:
    methods.no_verbose(sys, env)

if not environment["platform"] == "server":  # FIXME: detect GLES3
    environment.Append(
        BUILDERS={
            "GLES3_GLSL": environment.Builder(
                action=run_in_subprocess(gles_builders.build_gles3_headers),
                suffix="glsl.gen.h",
                src_suffix=".glsl",
            )
        }
    )
    environment.Append(
        BUILDERS={
            "GLES2_GLSL": environment.Builder(
                action=run_in_subprocess(gles_builders.build_gles2_headers),
                suffix="glsl.gen.h",
                src_suffix=".glsl",
            )
        }
    )

scons_cache_path = os.environ.get("SCONS_CACHE")
if scons_cache_path != None:
    CacheDir(scons_cache_path)
    print("Scons cache enabled... (path: '" + scons_cache_path + "')")

if environment["vsproj"]:
    environment.vs_incs = []
    environment.vs_srcs = []

Export("environment")

# build subdirs, the build order is dependent on link order.

SConscript("core/SCsub")
SConscript("servers/SCsub")
SConscript("scene/SCsub")
SConscript("editor/SCsub")
SConscript("projects_manager/SCsub")
SConscript("drivers/SCsub")
SConscript("platform/SCsub")
SConscript("modules/SCsub")
SConscript("main/SCsub")

# Build selected platform.
SConscript("platform/" + selected_platform + "/SCsub")

# Microsoft Visual Studio Project Generation.
if ARGUMENTS.get("vsproj", False):
    if os.name != "nt":
        print(
            "Error: The `vsproj` option is only usable on Windows with Visual Studio."
        )
        sys.exit(255)
    environment["CPPPATH"] = [Dir(path) for path in environment["CPPPATH"]]
    build.create_visual_studio_solution(environment, GetOption("num_jobs"))
    build.generate_cpp_hint_file("cpp.hint")
    sys.exit(0)


# Check for the existence of headers
conf = Configure(env)
if "check_c_headers" in env:
    for header in environment["check_c_headers"]:
        if conf.CheckCHeader(header[0]):
            environment.AppendUnique(CPPDEFINES=[header[1]])

# The following only makes sense when the 'env' is defined, and assumes it is.
if "env" in locals():
    methods.show_progress(env)
    # TODO: replace this with `environment.Dump(format="json")`
    # once we start requiring SCons 4.0 as min version.
    methods.dump(env)


def print_elapsed_time():
    elapsed_time_sec = round(time.time() - time_at_start, 3)
    time_ms = round((elapsed_time_sec % 1) * 1000)
    print(
        "[Time elapsed: {}.{:03}]".format(
            time.strftime("%H:%M:%S", time.gmtime(elapsed_time_sec)), time_ms
        )
    )


atexit.register(print_elapsed_time)
