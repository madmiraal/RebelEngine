def create_cpp_hint_file():
    filename = "cpp.hint"
    if os.path.isfile(filename):
        # Don't overwrite an existing cpp.hint file, because the user may have customized it.
        return
    try:
        with open(filename, "w") as file:
            file.write("#define GDCLASS(m_class, m_inherits)\n")
    except IOError:
        print("Could not create cpp.hint file.")


def create_visual_studio_solution(env, num_jobs):
    batch_file = find_visual_c_batch_file(env)
    if batch_file:

        def build_commandline(commands):
            common_build_prefix = [
                'cmd /V /C set "plat=$(PlatformTarget)"',
                '(if "$(PlatformTarget)"=="x64" (set "plat=x86_amd64"))',
                'set "tools=%s"' % env["tools"],
                '(if "$(Configuration)"=="release" (set "tools=no"))',
                'call "' + batch_file + '" !plat!',
            ]

            # windows allows us to have spaces in paths, so we need
            # to double quote off the directory. However, the path ends
            # in a backslash, so we need to remove this, lest it escape the
            # last double quote off, confusing MSBuild
            common_build_postfix = [
                "--directory=\"$(ProjectDir.TrimEnd('\\'))\"",
                "platform=windows",
                "target=$(Configuration)",
                "progress=no",
                "tools=!tools!",
                "-j%s" % num_jobs,
            ]

            if env["custom_modules"]:
                common_build_postfix.append("custom_modules=%s" % env["custom_modules"])

            result = " ^& ".join(
                common_build_prefix + [" ".join([commands] + common_build_postfix)]
            )
            return result

        add_to_vs_project(env, env.core_sources)
        add_to_vs_project(env, env.drivers_sources)
        add_to_vs_project(env, env.main_sources)
        add_to_vs_project(env, env.modules_sources)
        add_to_vs_project(env, env.projects_manager_sources)
        add_to_vs_project(env, env.scene_sources)
        add_to_vs_project(env, env.servers_sources)
        add_to_vs_project(env, env.editor_sources)

        for header in glob_recursive("**/*.h"):
            env.vs_incs.append(str(header))

        env["MSVSBUILDCOM"] = build_commandline("scons")
        env["MSVSREBUILDCOM"] = build_commandline("scons vsproj=yes")
        env["MSVSCLEANCOM"] = build_commandline("scons --clean")

        # This version information (Win32, x64, Debug, Release, Release_Debug seems to be
        # required for Visual Studio to understand that it needs to generate an NMAKE
        # project. Do not modify without knowing what you are doing.
        debug_variants = ["debug|Win32"] + ["debug|x64"]
        release_variants = ["release|Win32"] + ["release|x64"]
        release_debug_variants = ["release_debug|Win32"] + ["release_debug|x64"]
        variants = debug_variants + release_variants + release_debug_variants
        debug_targets = ["bin\\rebel.windows.tools.32.msvc.exe"] + [
            "bin\\rebel.windows.tools.64.msvc.exe"
        ]
        release_targets = ["bin\\rebel.windows.opt.32.msvc.exe"] + [
            "bin\\rebel.windows.opt.64.msvc.exe"
        ]
        release_debug_targets = ["bin\\rebel.windows.opt.tools.32.msvc.exe"] + [
            "bin\\rebel.windows.opt.tools.64.msvc.exe"
        ]
        targets = debug_targets + release_targets + release_debug_targets
        if not env.get("MSVS"):
            env["MSVS"]["PROJECTSUFFIX"] = ".vcxproj"
            env["MSVS"]["SOLUTIONSUFFIX"] = ".sln"
        env.MSVSProject(
            target=["Rebel Engine" + env["MSVSPROJECTSUFFIX"]],
            incs=env.vs_incs,
            srcs=env.vs_srcs,
            runfile=targets,
            buildtarget=targets,
            auto_build_solution=1,
            variant=variants,
        )
    else:
        print(
            "Could not locate Visual Studio batch file for setting up the build environment. Not generating VS project."
        )
