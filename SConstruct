#!/usr/bin/env python
import os
import sys

from methods import print_error


libname = "godot-openvr-overlay"
projectdir = "project"
addondir = os.path.join(projectdir, "addons", libname)

localEnv = Environment(tools=["default"], PLATFORM="")

# Build profiles can be used to decrease compile times.
# You can either specify "disabled_classes", OR
# explicitly specify "enabled_classes" which disables all other classes.
# Update build_profile.json as needed and uncomment the line below, or
# manually specify the build_profile parameter when running SCons.

# localEnv["build_profile"] = "build_profile.json"

customs = ["custom.py"]
customs = [os.path.abspath(path) for path in customs]

opts = Variables(customs, ARGUMENTS)
opts.Add("OPENVR_DIR", "Path to an OpenVR SDK checkout or release bundle", "")
opts.Update(localEnv)

Help(opts.GenerateHelpText(localEnv))

env = localEnv.Clone()

if not (os.path.isdir("godot-cpp") and os.listdir("godot-cpp")):
    print_error("""godot-cpp is not available within this folder, as Git submodules haven't been initialized.
Run the following command to download godot-cpp:

    git submodule update --init --recursive""")
    sys.exit(1)

env = SConscript("godot-cpp/SConstruct", {"env": env, "customs": customs})

openvr_dir = ARGUMENTS.get("OPENVR_DIR", env.get("OPENVR_DIR", os.path.abspath("openvr")))
openvr_include_dir = os.path.join(openvr_dir, "headers")

platform_lib_dirs = {
    "windows": {
        "x86_64": os.path.join(openvr_dir, "lib", "win64"),
        "x86_32": os.path.join(openvr_dir, "lib", "win32"),
    },
    "linux": {
        "x86_64": os.path.join(openvr_dir, "lib", "linux64"),
        "x86_32": os.path.join(openvr_dir, "lib", "linux32"),
        "arm64": os.path.join(openvr_dir, "lib", "linuxarm64"),
    },
    "macos": {
        "universal": os.path.join(openvr_dir, "lib", "osx32"),
        "x86_64": os.path.join(openvr_dir, "lib", "osx32"),
    },
}

platform_runtime_files = {
    "windows": {
        "x86_64": os.path.join(openvr_dir, "bin", "win64", "openvr_api.dll"),
        "x86_32": os.path.join(openvr_dir, "bin", "win32", "openvr_api.dll"),
    },
}

arch = env.get("arch", "x86_64")
platform = env["platform"]
openvr_lib_dir = platform_lib_dirs.get(platform, {}).get(arch)

if not os.path.isfile(os.path.join(openvr_include_dir, "openvr.h")) or not openvr_lib_dir or not os.path.isdir(openvr_lib_dir):
    print_error(
        "No valid OpenVR SDK was found for this build.\n"
        "Set OPENVR_DIR to an OpenVR SDK checkout or unpacked release containing headers/ and lib/."
    )
    sys.exit(1)

env.Append(CPPPATH=["src/"])
env.Append(CPPPATH=[openvr_include_dir])
env.Append(LIBPATH=[openvr_lib_dir])
env.Append(LIBS=["openvr_api"])

sources = Glob("src/*.cpp")

if env["target"] in ["editor", "template_debug"]:
    try:
        doc_data = env.GodotCPPDocData("src/gen/doc_data.gen.cpp", source=Glob("doc_classes/*.xml"))
        sources.append(doc_data)
    except AttributeError:
        print("Not including class reference as we're targeting a pre-4.3 baseline.")

# .dev doesn't inhibit compatibility, so we don't need to key it.
# .universal just means "compatible with all relevant arches" so we don't need to key it.
suffix = env['suffix'].replace(".dev", "").replace(".universal", "")

lib_filename = "{}{}{}{}".format(env.subst('$SHLIBPREFIX'), libname, suffix, env.subst('$SHLIBSUFFIX'))

library = env.SharedLibrary(
    "bin/{}/{}".format(env['platform'], lib_filename),
    source=sources,
)

copy = env.Install("{}/bin/{}/".format(addondir, env["platform"]), library)

default_args = [library, copy]

runtime_file = platform_runtime_files.get(platform, {}).get(arch)
if runtime_file and os.path.isfile(runtime_file):
    runtime_copy = env.Install("{}/bin/{}/".format(addondir, env["platform"]), runtime_file)
    default_args.append(runtime_copy)

Default(*default_args)
