
import os
import sys
import argparse
import shutil
import subprocess
import urllib.request
import tarfile
import zipfile
import glob
import multiprocessing

# ==============================================================================
# Configuration
# ==============================================================================

# ANSI Colors (Work on Linux/Mac, and Win10+ Terminals)
RED = '\033[0;31m'
GREEN = '\033[0;32m'
YELLOW = '\033[1;33m'
BLUE = '\033[0;34m'
NC = '\033[0m'

# Assumes script is in the project root or a subfolder. 
# Adjust if script is in src/scripts/ etc.
SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
PROJECT_ROOT = SCRIPT_DIR 

VERSIONS = {
    "FREETYPE": "2.14.1",
    "IMGUI": "1.92.5",
    "IMPLOT": "0.17",
    "PLUTOVG": "1.3.2",
    "LUNASVG": "3.5.0",
    "YOGA": "3.2.1",
    "SDL3": "3.4.0",
    "GLFW": "3.4",
    "BLEND2D": "0.21.2",
    "ASMJIT": "2024-11-18",
    "NFD": "1.3.0",
    "JSON": "3.12.0",
}

URLS = {
    "FREETYPE": f"https://gitlab.freedesktop.org/freetype/freetype/-/archive/VER-2-14-1/freetype-VER-2-14-1.tar.gz",
    "IMGUI": f"https://github.com/ocornut/imgui/archive/refs/tags/v{VERSIONS['IMGUI']}.tar.gz",
    "IMPLOT": f"https://github.com/epezent/implot/archive/refs/tags/v{VERSIONS['IMPLOT']}.tar.gz",
    "PLUTOVG": f"https://github.com/sammycage/plutovg/archive/refs/tags/v{VERSIONS['PLUTOVG']}.tar.gz",
    "LUNASVG": f"https://github.com/sammycage/lunasvg/archive/refs/tags/v{VERSIONS['LUNASVG']}.tar.gz",
    "YOGA": f"https://github.com/facebook/yoga/archive/refs/tags/v{VERSIONS['YOGA']}.tar.gz",
    "SDL3": f"https://github.com/libsdl-org/SDL/releases/download/release-{VERSIONS['SDL3']}/SDL3-devel-{VERSIONS['SDL3']}-VC.zip",
    "GLFW": f"https://github.com/glfw/glfw/releases/download/{VERSIONS['GLFW']}/glfw-{VERSIONS['GLFW']}.bin.WIN64.zip",
    "BLEND2D": f"https://blend2d.com/download/blend2d-{VERSIONS['BLEND2D']}.tar.gz",
    "ASMJIT": f"https://github.com/asmjit/asmjit/archive/refs/tags/{VERSIONS['ASMJIT']}.tar.gz",
    "NFD": f"https://github.com/btzy/nativefiledialog-extended/archive/refs/tags/v{VERSIONS['NFD']}.tar.gz",
    "JSON": f"https://github.com/nlohmann/json/archive/refs/tags/v{VERSIONS['JSON']}.tar.gz",
    "ICONFONT": "https://github.com/juliettef/IconFontCppHeaders/archive/refs/heads/main.zip",
    "STB_IMAGE": "https://raw.githubusercontent.com/nothings/stb/master/stb_image.h",
}

# ==============================================================================
# Helpers
# ==============================================================================

def log(msg, color=GREEN):
    print(f"{color}{msg}{NC}")

def error_exit(msg):
    print(f"{RED}[ERROR] {msg}{NC}")
    sys.exit(1)

def run_command(cmd, cwd=None, env=None, shell=True):
    try:
        # List-form commands must avoid shell mode; fixes CMake list args parsing.
        if isinstance(cmd, (list, tuple)):
            subprocess.check_call(cmd, cwd=cwd, env=env, shell=False)
        else:
            subprocess.check_call(cmd, cwd=cwd, env=env, shell=shell)
    except subprocess.CalledProcessError:
        error_exit(f"Command failed: {cmd}")

def download_file(url, dest):
    log(f"Downloading {url}...")
    try:
        urllib.request.urlretrieve(url, dest)
    except Exception as e:
        error_exit(f"Download failed: {e}")

def extract_archive(path, dest_dir):
    log(f"Extracting {path}...")
    if path.endswith(".zip"):
        with zipfile.ZipFile(path, 'r') as z:
            z.extractall(dest_dir)
    elif path.endswith(".tar.gz") or path.endswith(".tgz"):
        with tarfile.open(path, 'r:gz') as t:
            t.extractall(dest_dir)

def find_msbuild():
    """Locate MSBuild.exe via vswhere"""
    if "VSCMD_VER" in os.environ:
        pass  # Already in VS dev command prompt
    vswhere = os.path.expandvars(r"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe")
    if not os.path.exists(vswhere):
        error_exit("vswhere.exe not found. Install Visual Studio (2022 or later).")
    
    cmd = [vswhere, "-latest", "-requires", "Microsoft.Component.MSBuild", "-find", r"MSBuild\**\Bin\MSBuild.exe"]
    try:
        return subprocess.check_output(cmd, encoding='utf-8').strip().splitlines()[0]
    except:
        log("MSBuild not found via standard query, trying prerelease versions...", YELLOW)
        altcmd = [vswhere, "-latest", "-prerelease", "-requires", "Microsoft.Component.MSBuild", "-find", r"MSBuild\**\Bin\MSBuild.exe"]
        try:
            return subprocess.check_output(altcmd, encoding='utf-8').strip().splitlines()[0]
        except:
            error_exit("MSBuild not found.")

def get_vc_env():
    """Get Visual Studio environment variables for CL.exe and CMake generator"""
    if "VSCMD_VER" in os.environ:
        pass  # Already in VS dev command prompt
    vswhere = os.path.expandvars(r"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe")
    try:
        install_path = subprocess.check_output([vswhere, "-latest", "-property", "installationPath"], encoding='utf-8').strip()
        version = subprocess.check_output([vswhere, "-latest", "-property", "installationVersion"], encoding='utf-8').strip()
    except:
        log("Installation path not found via standard query, trying prerelease versions...", YELLOW)
        altcmd = [vswhere, "-latest", "-prerelease", "-property", "installationPath"]
        try:
            install_path = subprocess.check_output(altcmd, encoding='utf-8').strip().splitlines()[0]
            version = subprocess.check_output([vswhere, "-latest", "-prerelease", "-property", "installationVersion"], encoding='utf-8').strip().splitlines()[0]
        except:
            error_exit("VS Installation path not found.")

    if install_path == "":
        log("Installation path not found via standard query, trying prerelease versions...", YELLOW)
        altcmd = [vswhere, "-latest", "-prerelease", "-property", "installationPath"]
        try:
            install_path = subprocess.check_output(altcmd, encoding='utf-8').strip().splitlines()[0]
            version = subprocess.check_output([vswhere, "-latest", "-prerelease", "-property", "installationVersion"], encoding='utf-8').strip().splitlines()[0]
        except:
            error_exit("VS Installation path not found.")
        
    vcvars = os.path.join(install_path, "VC", "Auxiliary", "Build", "vcvarsall.bat")
    if not os.path.exists(vcvars):
        error_exit("vcvarsall.bat not found.")
        
    # Run vcvarsall and dump environment
    cmd = f'"{vcvars}" amd64 && set'
    try:
        output = subprocess.check_output(cmd, shell=True, encoding='utf-8', errors='ignore')
        env = {}
        for line in output.splitlines():
            if '=' in line:
                k, v = line.split('=', 1)
                env[k] = v
    except:
        error_exit("Failed to initialize VS environment.")
    
    # Determine CMake generator
    major = int(version.split('.')[0])
    if major == 18:
        generator = "Visual Studio 18 2026"
    elif major == 17:
        generator = "Visual Studio 17 2022"
    elif major == 16:
        generator = "Visual Studio 16 2019"
    else:
        error_exit(f"Unsupported VS version: {version}")
    
    sln_ext = "sln" if major < 18 else "slnx"
    
    return env, generator, sln_ext

# ==============================================================================
# Linux GCC Setup
# ==============================================================================
def setup_linux_toolchain():
    """
    Handles GCC setup for RHEL (Toolsets) vs Ubuntu/Fedora (Native).
    """
    # 1. Check for RHEL/CentOS Toolsets (Legacy support)
    toolsets = [
        "/opt/rh/gcc-toolset-12/enable",
        "/opt/rh/gcc-toolset-11/enable",
        "/opt/rh/devtoolset-11/enable"
    ]
    
    enable_script = None
    for ts in toolsets:
        if os.path.exists(ts):
            enable_script = ts
            break
            
    env = os.environ.copy()
    
    if enable_script:
        print(f"{YELLOW}Enabling RHEL Toolset: {enable_script}{NC}")
        cmd = f"source {enable_script} && env"
        try:
            result = subprocess.run(cmd, shell=True, executable='/bin/bash', capture_output=True, text=True)
            if result.returncode == 0:
                for line in result.stdout.splitlines():
                    if '=' in line:
                        key, _, val = line.partition('=')
                        env[key] = val
        except:
            pass
        # Ensure cmake/gcc use the toolset compiler from the updated PATH.
        env["CC"] = env.get("CC", "gcc")
        env["CXX"] = env.get("CXX", "g++")
    else:
        # 2. Ubuntu/Fedora/Arch check
        try:
            gcc_out = subprocess.check_output(["gcc", "--version"], text=True).splitlines()[0]
            if "Ubuntu" in gcc_out or "Fedora" in gcc_out or "Free Software Foundation" in gcc_out:
                print(f"{GREEN}Using System GCC: {gcc_out}{NC}")
            else:
                print(f"{YELLOW}Warning: Using system GCC (Unknown Distro): {gcc_out}{NC}")
        except FileNotFoundError:
            print(f"{RED}Error: GCC not found. Please install build-essential (Ubuntu) or gcc-c++ (Fedora).{NC}")
            sys.exit(1)
            
    return env

# ==============================================================================
# Main
# ==============================================================================

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--platform", default="sdl3", choices=["test", "sdl3", "glfw"])
    parser.add_argument("--release", "-r", action="store_true")
    parser.add_argument("--debug", "-d", action="store_true")
    parser.add_argument("--update", action="store_true")
    parser.add_argument("--disable-plots", action="store_true")
    parser.add_argument("--disable-svg", action="store_true")
    parser.add_argument("--disable-images", action="store_true")
    parser.add_argument("--disable-icon-font", action="store_true")
    parser.add_argument("--disable-richtext", action="store_true")
    parser.add_argument("--enable-blend2d", action="store_true")
    parser.add_argument("--clean", action="store_true")
    args = parser.parse_args()

    # Settings
    is_debug = args.debug
    build_type = "Debug" if is_debug else "Release"
    lib_subdir = "debug" if is_debug else "release"
    update_all = args.update
    args.clean = True if update_all else args.clean
    
    # Library extension by platform
    LIB_EXT = ".lib" if sys.platform == 'win32' else ".a"
    LIB_PREFIX = "" if sys.platform == 'win32' else "lib"
    # Paths
    lib_output_dir = os.path.join(PROJECT_ROOT, "src", "libs", "lib", "win32" if sys.platform == 'win32' else "linux", lib_subdir)
    libs_header_dir = os.path.join(PROJECT_ROOT, "src", "libs", "inc")
    dependency_dir = os.path.join(PROJECT_ROOT, "dependency")
    
    # Create Directories
    for d in [lib_output_dir, libs_header_dir, dependency_dir]:
        if not os.path.exists(d): os.makedirs(d)

    log(f"Platform: {args.platform} | Type: {build_type}")
    log(f"Output: {lib_output_dir}")

    # Feature Flags
    feats = {
        "IMGUI": True, "FREETYPE": True, "YOGA": True,
        "IMPLOT": False, "PLUTOVG": False, "LUNASVG": False,
        "SDL3": False, "GLFW": False, "BLEND2D": False,
        "NFD": False, "JSON": False, "STB": False, "ICONS": False
    }
    
    if args.platform == "sdl3":
        feats.update({"IMPLOT": True, "PLUTOVG": True, "LUNASVG": True, "SDL3": True, "BLEND2D": True, "STB": True, "ICONS": True})
    elif args.platform == "glfw":
        feats.update({"IMPLOT": True, "PLUTOVG": True, "LUNASVG": True, "GLFW": True, "NFD": True, "STB": True, "ICONS": True})

    # Apply overrides
    if args.disable_plots: feats["IMPLOT"] = False
    if args.disable_svg: feats["PLUTOVG"] = feats["LUNASVG"] = False
    if args.disable_images: feats["STB"] = False
    if args.disable_icon_font: feats["ICONS"] = False
    if not args.enable_blend2d: feats["BLEND2D"] = False

    # Setup Tools
    if sys.platform == 'win32':
        msbuild = find_msbuild()
        vc_env, cmake_generator, sln_ext = get_vc_env()
        os.environ.update(vc_env)  # Apply VS environment to subprocesses.
    else:
        vc_env = setup_linux_toolchain()
        os.environ.update(vc_env)  # Apply toolset env to pick newer GCC for C++20.
        cmake_generator = None
        sln_ext = None
    
    os.chdir(dependency_dir)

     # Copy Include
    def copy_tree(src, dst):
        if os.path.isdir(src):
            if not os.path.isdir(dst): os.makedirs(dst)
            for item in os.listdir(src):
                copy_tree(os.path.join(src, item), os.path.join(dst, item))
        else:
            shutil.copy2(src, dst)

    # Remove previous artifacts for Windows
    if sys.platform == 'win32':
        obj_files_dir = os.path.join(PROJECT_ROOT, "build\\glimmer_sdl3.dir", build_type)
        if os.path.exists(obj_files_dir):
            lib_files = os.listdir(obj_files_dir)
            for item in lib_files:
                if item.endswith(".obj") or item.endswith(".lib"):
                    try:
                        os.remove(os.path.join(obj_files_dir, item))
                    except:
                        pass

        try:
            os.remove(os.path.join(PROJECT_ROOT, "staticlib", build_type, "glimmer_sdl3.lib"))
            os.remove(os.path.join(PROJECT_ROOT, "staticlib", build_type, "glimmer_sdl3.pdb"))
        except:
            pass

        try:
            os.remove(os.path.join(PROJECT_ROOT, "staticlib", "combined", build_type, "glimmer.lib"))
        except:
            pass

    # -------------------------------------------------------------------------
    # 1. FreeType
    # -------------------------------------------------------------------------
    if feats["FREETYPE"]:
        if update_all or not os.path.exists(os.path.join(lib_output_dir, f"freetype{LIB_EXT}")):
            log("Building FreeType...")
            dirname = "freetype-VER-2-14-1"
            if not os.path.exists(dirname):
                download_file(URLS["FREETYPE"], "freetype.tar.gz")
                extract_archive("freetype.tar.gz", ".")
            os.chdir(dirname)
            if not os.path.exists("build"): os.makedirs("build")
            os.chdir("build")
            # Force CMake to re-detect compilers when switching toolsets
            if os.path.exists("CMakeCache.txt"):
                os.remove("CMakeCache.txt")
            if sys.platform == 'win32':
                run_command(f'cmake .. -G "{cmake_generator}" -A x64 -DCMAKE_BUILD_TYPE={build_type} -DBUILD_SHARED_LIBS=OFF -DFT_DISABLE_ZLIB=ON -DFT_DISABLE_BZIP2=ON -DFT_DISABLE_PNG=ON -DFT_DISABLE_HARFBUZZ=ON -DFT_DISABLE_BROTLI=ON')
                run_command(f'"{msbuild}" freetype.{sln_ext} /p:Configuration={build_type} /p:Platform=x64 /m')
            else:
                run_command(f'cmake .. -DCMAKE_BUILD_TYPE={build_type} -DBUILD_SHARED_LIBS=OFF -DFT_DISABLE_ZLIB=ON -DFT_DISABLE_BZIP2=ON -DFT_DISABLE_PNG=OFF -DFT_DISABLE_HARFBUZZ=ON -DFT_DISABLE_BROTLI=ON')
                run_command('cmake --build . --config {0}'.format(build_type))
            
            # Copy Lib (single-config generators may not create Release/ subdir).
            if sys.platform == 'win32':
                lib_src = f"{build_type}/{LIB_PREFIX}freetype{LIB_EXT}" if build_type == "Release" else f"{build_type}/{LIB_PREFIX}freetyped{LIB_EXT}"
            else:
                # Single-config generators put outputs in the build dir (no Release/Debug subdir).
                candidates = [
                    f"{build_type}/{LIB_PREFIX}freetype{LIB_EXT}",
                    f"{LIB_PREFIX}freetype{LIB_EXT}",
                    f"{build_type}/{LIB_PREFIX}freetyped{LIB_EXT}",
                    f"{LIB_PREFIX}freetyped{LIB_EXT}",
                ]
                lib_src = next((p for p in candidates if os.path.exists(p)), None)
                if lib_src is None:
                    raise FileNotFoundError(
                        f"FreeType library not found. Tried: {', '.join(candidates)}"
                    )
            shutil.copy(lib_src, os.path.join(lib_output_dir, f"{LIB_PREFIX}freetype{LIB_EXT}"))
                    
            log("Copying FreeType Headers...")
            copy_tree(os.path.abspath("../include"), os.path.join(libs_header_dir, "freetype2"))
            os.chdir(dependency_dir)

    # -------------------------------------------------------------------------
    # 2. Yoga
    # -------------------------------------------------------------------------
    if feats["YOGA"]:
        if update_all or not os.path.exists(os.path.join(lib_output_dir, f"yoga{LIB_EXT}")):
            log("Building Yoga...")
            dirname = f"yoga-{VERSIONS['YOGA']}"
            if not os.path.exists(dirname):
                download_file(URLS["YOGA"], "yoga.tar.gz")
                extract_archive("yoga.tar.gz", ".")
            os.chdir(dirname)
            if not os.path.exists("build"): os.makedirs("build")
            os.chdir("build")
            if sys.platform == 'win32':
                run_command(f'cmake .. -G "{cmake_generator}" -A x64 -DCMAKE_BUILD_TYPE={build_type} -DBUILD_SHARED_LIBS=OFF')
                run_command(f'"{msbuild}" yoga/yogacore.{sln_ext} /p:Configuration={build_type} /p:Platform=x64 /m')
            else:
                # Use toolset compiler to ensure C++20 headers like <bit> are available.
                cc = vc_env.get("CC", "gcc")
                cxx = vc_env.get("CXX", "g++")
                run_command(
                    f'cmake .. -DCMAKE_BUILD_TYPE={build_type} -DBUILD_SHARED_LIBS=OFF '
                    f'-DCMAKE_C_COMPILER={cc} -DCMAKE_CXX_COMPILER={cxx}',
                    env=vc_env
                )
                run_command('cmake --build . --config {0}'.format(build_type), env=vc_env)
            
            # Copy (single-config generators may output in yoga/ instead of yoga/Release).
            if sys.platform == 'win32':
                lib_src = f"yoga/{build_type}/{LIB_PREFIX}yogacore{LIB_EXT}"
            else:
                candidates = [
                    f"yoga/{build_type}/{LIB_PREFIX}yogacore{LIB_EXT}",
                    f"yoga/{LIB_PREFIX}yogacore{LIB_EXT}",
                ]
                lib_src = next((p for p in candidates if os.path.exists(p)), None)
                if lib_src is None:
                    raise FileNotFoundError(
                        f"Yoga library not found. Tried: {', '.join(candidates)}"
                    )
            shutil.copy(lib_src, os.path.join(lib_output_dir, f"{LIB_PREFIX}yoga{LIB_EXT}"))
            
            # Header
            log("Copying Yoga Headers...")
            yoga_inc = os.path.join(libs_header_dir, "yoga")
            if not os.path.exists(yoga_inc): os.makedirs(yoga_inc)
            for h in glob.glob(os.path.join("..", "yoga", "*.h")): shutil.copy(h, yoga_inc)
            os.chdir(dependency_dir)

    # -------------------------------------------------------------------------
    # 4. LunaSVG
    # -------------------------------------------------------------------------
    if feats["LUNASVG"]:
        if update_all or not os.path.exists(os.path.join(lib_output_dir, f"lunasvg{LIB_EXT}")):
            log("Building LunaSVG...")
            dirname = f"lunasvg-{VERSIONS['LUNASVG']}"
            if not os.path.exists(dirname):
                download_file(URLS["LUNASVG"], "lunasvg.tar.gz")
                extract_archive("lunasvg.tar.gz", ".")
            os.chdir(dirname)
            if not os.path.exists("build"): os.makedirs("build")
            os.chdir("build")
            if sys.platform == 'win32':
                run_command(f'cmake .. -G "{cmake_generator}" -A x64 -DCMAKE_BUILD_TYPE={build_type} -DBUILD_SHARED_LIBS=OFF -DLUNASVG_BUILD_EXAMPLES=OFF')
                run_command(f'"{msbuild}" lunasvg.{sln_ext} /p:Configuration={build_type} /p:Platform=x64 /m')
            else:
                run_command(f'cmake .. -DCMAKE_BUILD_TYPE={build_type} -DBUILD_SHARED_LIBS=OFF -DLUNASVG_BUILD_EXAMPLES=OFF')
                run_command('cmake --build . --config {0}'.format(build_type))
            
            # Copy (single-config generators may output in build root).
            if sys.platform == 'win32':
                luna_src = f"{build_type}/{LIB_PREFIX}lunasvg{LIB_EXT}"
                pluto_src = f"plutovg/{build_type}/{LIB_PREFIX}plutovg{LIB_EXT}"
            else:
                luna_candidates = [
                    f"{build_type}/{LIB_PREFIX}lunasvg{LIB_EXT}",
                    f"{LIB_PREFIX}lunasvg{LIB_EXT}",
                ]
                pluto_candidates = [
                    f"plutovg/{build_type}/{LIB_PREFIX}plutovg{LIB_EXT}",
                    f"plutovg/{LIB_PREFIX}plutovg{LIB_EXT}",
                ]
                luna_src = next((p for p in luna_candidates if os.path.exists(p)), None)
                pluto_src = next((p for p in pluto_candidates if os.path.exists(p)), None)
                if luna_src is None:
                    raise FileNotFoundError(
                        f"LunaSVG library not found. Tried: {', '.join(luna_candidates)}"
                    )
                if pluto_src is None:
                    raise FileNotFoundError(
                        f"PlutoVG library not found. Tried: {', '.join(pluto_candidates)}"
                    )
            shutil.copy(luna_src, os.path.join(lib_output_dir, f"{LIB_PREFIX}lunasvg{LIB_EXT}"))
            shutil.copy(pluto_src, os.path.join(lib_output_dir, f"{LIB_PREFIX}plutovg{LIB_EXT}"))
            # Header is single file usually

            log("Copying LunaSVG Headers...")
            inc_dir = os.path.join(libs_header_dir, "lunasvg")
            if not os.path.exists(inc_dir): os.makedirs(inc_dir)
            if os.path.exists("../include/lunasvg.h"): shutil.copy("../include/lunasvg.h", inc_dir)
            os.chdir(dependency_dir)

    # -------------------------------------------------------------------------
    # 5. SDL3 (Prebuilt on Windows, build on Linux)
    # -------------------------------------------------------------------------
    if feats["SDL3"]:
        if update_all or not os.path.exists(os.path.join(lib_output_dir, f"SDL3{LIB_EXT}")):
            log("Setting up SDL3...")

            if sys.platform == 'win32':
                download_file("https://github.com/libsdl-org/SDL/releases/download/release-3.4.0/SDL3-3.4.0.zip", "sdl3.zip")
                extract_archive("sdl3.zip", ".")
                os.chdir("SDL3-3.4.0")
                dirname="SDL3-3.4.0"
                if not os.path.exists("build"): os.makedirs("build")
                os.chdir("build")
                run_command(f'cmake .. -G "{cmake_generator}" -A x64 -DCMAKE_BUILD_TYPE={build_type}' 
                            '-DBUILD_SHARED_LIBS=OFF -DSDL_STATIC=ON '
                            '-DSDL_SHARED=OFF -DSDL_TESTS=OFF -DSDL_TEST_LIBRARY=OFF')
                run_command(f'"{msbuild}" SDL3.{sln_ext} /p:Configuration={build_type} /p:Platform=x64 /m')
                shutil.copy(os.path.join(build_type, f"SDL3-static.lib"), os.path.join(lib_output_dir, f"{LIB_PREFIX}SDL3{LIB_EXT}"))
                os.chdir("..")
            else:
                download_file("https://github.com/libsdl-org/SDL/releases/download/release-3.4.0/SDL3-3.4.0.tar.gz", "sdl3.tar.gz")
                extract_archive("sdl3.tar.gz", ".")
                os.chdir("SDL3-3.4.0")
                dirname="SDL3-3.4.0"
                if not os.path.exists("build"): os.makedirs("build")
                os.chdir("build")
                # Force CMake to re-detect compilers when switching toolsets.
                if os.path.exists("CMakeCache.txt"):
                    os.remove("CMakeCache.txt")
                # Use toolset compiler for consistency with Yoga and C++20 support.
                cc = vc_env.get("CC", "gcc")
                cxx = vc_env.get("CXX", "g++")
                run_command(
                    f'cmake .. -DCMAKE_BUILD_TYPE={build_type} '
                    f'-DCMAKE_C_COMPILER={cc} -DCMAKE_CXX_COMPILER={cxx} '
                    '-DBUILD_SHARED_LIBS=OFF -DSDL_STATIC=ON '
                    '-DSDL_SHARED=OFF -DSDL_TEST=OFF -DSDL_X11=ON '
                    '-DSDL_X11_XRANDR=ON -DSDL_X11_XSCRNSAVER=ON '
                    '-DSDL_X11_XTEST=OFF -DSDL_WAYLAND=OFF '
                    '-DSDL_TESTS=OFF -DSDL_TEST_LIBRARY=OFF '
                    '-DSDL_OPENGL=ON',
                    env=vc_env
                )
                run_command('make -j$(nproc)', env=vc_env)
                # SDL3 static lib is built in build root, not always in src/.
                sdl_candidates = [
                    os.path.join("src", f"libSDL3{LIB_EXT}"),
                    f"libSDL3{LIB_EXT}",
                ]
                sdl_src = next((p for p in sdl_candidates if os.path.exists(p)), None)
                if sdl_src is None:
                    raise FileNotFoundError(
                        f"SDL3 library not found. Tried: {', '.join(sdl_candidates)}"
                    )
                shutil.copy(sdl_src, os.path.join(lib_output_dir, f"{LIB_PREFIX}SDL3{LIB_EXT}"))
                os.chdir("..")

            log("Copying SDL3 Headers...")
            # SDL3 include layout differs between archive and build tree.
            sdl_include_candidates = [
                os.path.join(dirname, "include", "SDL3"),
                os.path.join("include", "SDL3"),
                os.path.join(dirname, "include"),
                "include",
            ]
            sdl_include_src = next((p for p in sdl_include_candidates if os.path.exists(p)), None)
            if sdl_include_src is None:
                raise FileNotFoundError(
                    f"SDL3 headers not found. Tried: {', '.join(sdl_include_candidates)}"
                )
            copy_tree(sdl_include_src, os.path.join(libs_header_dir, "SDL3"))
            os.chdir(dependency_dir)  # Return to dependency root so later downloads go to dependency/.

    # -------------------------------------------------------------------------
    # 6. GLFW (Prebuilt on Windows, build on Linux)
    # -------------------------------------------------------------------------
    if feats["GLFW"]:
        if update_all or not os.path.exists(os.path.join(lib_output_dir, f"glfw3{LIB_EXT}")):
            log("Setting up GLFW...")

            if sys.platform == 'win32':
                dirname = f"glfw-{VERSIONS['GLFW']}.bin.WIN64"
                if not os.path.exists(dirname):
                    download_file(URLS["GLFW"], "glfw.zip")
                    extract_archive("glfw.zip", ".")
                
                shutil.copy(os.path.join(dirname, "lib-vc2022", f"glfw3{LIB_EXT}"), os.path.join(lib_output_dir, f"{LIB_PREFIX}glfw3{LIB_EXT}"))
                shutil.copy(os.path.join(dirname, "lib-vc2022", f"glfw3_mt{LIB_EXT}"), os.path.join(lib_output_dir, f"{LIB_PREFIX}glfw3_mt{LIB_EXT}"))
            else:
                download_file("https://github.com/glfw/glfw/archive/refs/tags/3.4.tar.gz", "glfw.tar.gz")
                extract_archive("glfw.tar.gz", ".")
                dirname="glfw-3.4"
                os.chdir("glfw-3.4")
                if not os.path.exists("build"): os.makedirs("build")
                os.chdir("build")
                run_command(f'cmake .. -DCMAKE_BUILD_TYPE={build_type} -DBUILD_SHARED_LIBS=OFF -DGLFW_BUILD_TESTS=OFF -DGLFW_BUILD_EXAMPLES=OFF -DGLFW_BUILD_DOCS=OFF')
                run_command('make -j$(nproc)')
                shutil.copy(os.path.join("src", f"libglfw3{LIB_EXT}"), os.path.join(lib_output_dir, f"{LIB_PREFIX}glfw3{LIB_EXT}"))
                os.chdir("..")
                
            log("Copying GLFW Headers...")
            copy_tree(os.path.join(dirname, "include", "GLFW"), os.path.join(libs_header_dir, "GLFW"))

    # -------------------------------------------------------------------------
    # 7. Blend2D (disabled on Windows for now)
    # -------------------------------------------------------------------------
    if feats["BLEND2D"] and sys.platform != 'win32':
        if update_all or not os.path.exists(os.path.join(lib_output_dir, f"blend2d{LIB_EXT}")):
            log("Building Blend2D...")
            dirname = "blend2d"
            if not os.path.exists(dirname):
                download_file(URLS["BLEND2D"], "blend2d.tar.gz")
                extract_archive("blend2d.tar.gz", ".")
                #if os.path.exists("blend2d") and not os.path.exists(dirname): os.rename("blend2d", dirname)
            
            os.chdir(dirname)
            if not os.path.exists("build"): os.makedirs("build")
            os.chdir("build")
            
            if sys.platform == 'win32':
                cmake_cmd = (f'cmake .. -G "{cmake_generator}" -A x64 -DCMAKE_BUILD_TYPE={build_type} '
                         '-DBUILD_SHARED_LIBS=OFF -DBLEND2D_STATIC=ON -DBLEND2D_TEST=OFF '
                         '-DBL_BUILD_OPT_AVX512=ON -DASMJIT_NO_FOREIGN=ON -DASMJIT_NO_STDCXX=ON '
                         '-DASMJIT_ABI_NAMESPACE=abi_bl -DASMJIT_STATIC=ON -DASMJIT_NO_AARCH64=ON')
                run_command(cmake_cmd)
                run_command(f'"{msbuild}" blend2d.{sln_ext} /p:Configuration={build_type} /p:Platform=x64 /m')
            else:
                cmake_cmd = (f'cmake .. -DCMAKE_BUILD_TYPE={build_type} '
                         '-DBUILD_SHARED_LIBS=OFF -DBLEND2D_STATIC=ON -DBLEND2D_TEST=OFF '
                         '-DBL_BUILD_OPT_AVX512=ON -DASMJIT_NO_FOREIGN=ON -DASMJIT_NO_STDCXX=ON '
                         '-DASMJIT_ABI_NAMESPACE=abi_bl -DASMJIT_STATIC=ON -DASMJIT_NO_AARCH64=ON')
                run_command(cmake_cmd)
                run_command('cmake --build . --config {0}'.format(build_type))
            
            shutil.copy(f"{build_type}/{LIB_PREFIX}blend2d{LIB_EXT}", os.path.join(lib_output_dir, f"{LIB_PREFIX}blend2d{LIB_EXT}"))
            
            # Headers
            log("Copying Blend2D Headers...")
            b2d_inc = os.path.join(libs_header_dir, "blend2d")
            os.chdir("..")
            # Logic from batch file: check root blend2d or src/blend2d
            if os.path.exists("blend2d/blend2d.h") and not os.path.exists(b2d_inc):
                shutil.copytree(
                    "blend2d",
                    b2d_inc,
                    ignore=shutil.ignore_patterns('*.cpp')
                )
            else:
                # Newer versions might differ, fallback to generic include copy if needed
                pass 
            os.chdir(dependency_dir)

    # -------------------------------------------------------------------------
    # 8. Build ImGui (Unified CMake Build with ImPlot)
    # -------------------------------------------------------------------------
    if feats["IMGUI"]:
        # We check for the final library name (imgui_static.lib based on CMake)
        if update_all or not os.path.exists(os.path.join(lib_output_dir, f"imgui_static{LIB_EXT}")):
            log("Building ImGui (and ImPlot if enabled) via CMake...")
            
            # 1. Download/Extract ImGui (re-extract if partial to avoid missing imgui.cpp).
            imgui_ver = VERSIONS['IMGUI']
            imgui_dir_name = f"imgui-{imgui_ver}"
            imgui_cpp = os.path.join(imgui_dir_name, "imgui.cpp")
            if not os.path.exists(imgui_dir_name) or not os.path.exists(imgui_cpp):
                if os.path.exists(imgui_dir_name):
                    shutil.rmtree(imgui_dir_name)
                if not os.path.exists("imgui.tar.gz"):
                    download_file(URLS["IMGUI"], "imgui.tar.gz")
                extract_archive("imgui.tar.gz", ".")
            if not os.path.exists(imgui_cpp):
                error_exit(f"ImGui source missing: {imgui_cpp}")
            
            # 2. Download/Extract ImPlot (re-extract if partial to avoid missing implot.cpp).
            implot_dir_name = f"implot-{VERSIONS['IMPLOT']}"
            implot_cpp = os.path.join(implot_dir_name, "implot.cpp")
            if feats["IMPLOT"]:
                if not os.path.exists(implot_dir_name) or not os.path.exists(implot_cpp):
                    if os.path.exists(implot_dir_name):
                        shutil.rmtree(implot_dir_name)
                    if not os.path.exists("implot.tar.gz"):
                        download_file(URLS["IMPLOT"], "implot.tar.gz")
                    extract_archive("implot.tar.gz", ".")
                if not os.path.exists(implot_cpp):
                    error_exit(f"ImPlot source missing: {implot_cpp}")

            # 4. Run CMake
            # We create a specific build directory for this CMake project
            cmake_build_dir = "build_imgui_cmake"
            if os.path.exists(cmake_build_dir): shutil.rmtree(cmake_build_dir)
            os.makedirs(cmake_build_dir)
            os.chdir(cmake_build_dir)

            # Resolve absolute paths for CMake
            source_cmake_path = os.path.join(PROJECT_ROOT, "imgui_cmake")
            abs_imgui_dir = os.path.join(dependency_dir, imgui_dir_name)
            abs_implot_dir = os.path.join(dependency_dir, implot_dir_name)
            abs_include_dir = libs_header_dir # Contains SDL3/GLFW/freetype2
            abs_freetype_inc = os.path.join(libs_header_dir, "freetype2")

            # Build CMake Arguments
            cmake_args = [
                f'cmake "{source_cmake_path}"',
                f'-G "{cmake_generator}" -A x64' if sys.platform == 'win32' else '',
                f'-DCMAKE_BUILD_TYPE={build_type}',
                f'-DIMGUI_DIR="{abs_imgui_dir}"',
                f'-DINCLUDE_DIR="{abs_include_dir}"', # Where SDL3/GLFW headers live
                f'-DFREETYPE_INCLUDE_DIR="{abs_freetype_inc}"'
            ]

            # Toggle Features
            cmake_args.append(f'-DBUILD_IMPLOT={"ON" if feats["IMPLOT"] else "OFF"}')
            if feats["IMPLOT"]:
                cmake_args.append(f'-DIMPLOT_DIR="{abs_implot_dir}"')

            # Platform Selection
            if args.platform == "sdl3":
                cmake_args.append('-DBUILD_WITH_SDL3=ON')
                cmake_args.append('-DBUILD_WITH_GLFW=OFF')
                if (args.enable_blend2d):
                    cmake_args.append('-DENABLE_BLEND2D=ON')
            elif args.platform == "glfw":
                cmake_args.append('-DBUILD_WITH_SDL3=OFF')
                cmake_args.append('-DBUILD_WITH_GLFW=ON')
            else:
                # Test platform or minimal
                cmake_args.append('-DBUILD_WITH_SDL3=OFF')
                cmake_args.append('-DBUILD_WITH_GLFW=OFF')

            # Execute CMake Generation
            run_command(" ".join(cmake_args))

            # Execute Build
            # Note: The target name is 'imgui_static' as defined in CMakeLists.txt
            if sys.platform == 'win32':
                run_command(f'"{msbuild}" ImGuiStaticLib.{sln_ext} /p:Configuration={build_type} /p:Platform=x64 /m')
            else:
                run_command('cmake --build . --config {0}'.format(build_type))

            # 5. Copy Artifacts
            # CMake output is usually in Release/ or Debug/ subdir, but can be in lib/.
            built_candidates = [
                os.path.join("lib", build_type, f"{LIB_PREFIX}imgui_static{LIB_EXT}"),
                os.path.join("lib", f"{LIB_PREFIX}imgui_static{LIB_EXT}"),
                f"{LIB_PREFIX}imgui_static{LIB_EXT}",
            ]
            built_lib = next((p for p in built_candidates if os.path.exists(p)), None)
            if built_lib:
                shutil.copy(built_lib, lib_output_dir)
                if is_debug:
                    for pdb in glob.glob(f"{build_type}/*.pdb"):
                        shutil.copy(pdb, lib_output_dir)
            else:
                error_exit("Build successful but imgui.lib not found.")

            # 6. Copy Headers (Manual Step required as Static Libs don't carry headers)
            log("Copying ImGui (ImPlot) Headers...")
            
            # A. ImGui Headers
            dest_inc_imgui = os.path.join(libs_header_dir, "imgui")
            if not os.path.exists(dest_inc_imgui): os.makedirs(dest_inc_imgui)
            
            for h in glob.glob(os.path.join(abs_imgui_dir, "*.h")):
                shutil.copy(h, dest_inc_imgui)

            if args.platform == "sdl3":
                imgui_sdl_dest = os.path.join(libs_header_dir, "imguisdl3")
                if os.path.exists(imgui_sdl_dest) == False: os.makedirs(imgui_sdl_dest)
                shutil.copy(os.path.join(abs_imgui_dir, "backends", "imgui_impl_sdl3.h"), imgui_sdl_dest)
                shutil.copy(os.path.join(abs_imgui_dir, "backends", "imgui_impl_sdlgpu3.h"), imgui_sdl_dest)
                shutil.copy(os.path.join(abs_imgui_dir, "backends", "imgui_impl_sdlrenderer3.h"), imgui_sdl_dest)
            elif args.platform == "glfw":
                imgui_glfw_dest = os.path.join(libs_header_dir, "imguiglfw")
                if os.path.exists(imgui_glfw_dest) == False: os.makedirs(imgui_glfw_dest)
                shutil.copy(os.path.join(abs_imgui_dir, "backends", "imgui_impl_glfw.h"), imgui_glfw_dest)
                shutil.copy(os.path.join(abs_imgui_dir, "backends", "imgui_impl_opengl3.h"), imgui_glfw_dest)
                shutil.copy(os.path.join(abs_imgui_dir, "backends", "imgui_impl_opengl3_loader.h"), imgui_glfw_dest)

            # Copy Misc
            copy_tree(os.path.join(abs_imgui_dir, "misc"), os.path.join(dest_inc_imgui, "misc"))

            # C. ImPlot Headers (if enabled)
            if feats["IMPLOT"]:
                dest_inc_implot = os.path.join(libs_header_dir, "implot")
                if not os.path.exists(dest_inc_implot): os.makedirs(dest_inc_implot)
                for h in glob.glob(os.path.join(abs_implot_dir, "*.h")):
                    shutil.copy(h, dest_inc_implot)

            os.chdir(dependency_dir)

    # -------------------------------------------------------------------------
    # 9. Build NFD-Extended
    # -------------------------------------------------------------------------
    if feats["NFD"]:
        if update_all or not os.path.exists(os.path.join(lib_output_dir, f"nfd{LIB_EXT}")):
            log(f"Building NFD-Extended v{VERSIONS['NFD']}...")
            
            dirname = f"nativefiledialog-extended-{VERSIONS['NFD']}"
            if not os.path.exists(dirname):
                download_file(URLS["NFD"], "nfd.tar.gz")
                extract_archive("nfd.tar.gz", ".")
            
            os.chdir(dirname)
            if not os.path.exists("build"): os.makedirs("build")
            os.chdir("build")
            
            # Configure with tests disabled
            
            # Build
            if sys.platform == 'win32':
                run_command(f'cmake .. -G "{cmake_generator}" -A x64 -DCMAKE_BUILD_TYPE={build_type} -DBUILD_SHARED_LIBS=OFF -DNFD_BUILD_TESTS=OFF')
                run_command(f'"{msbuild}" nfd.{sln_ext} /p:Configuration={build_type} /p:Platform=x64 /m')
            else:
                run_command(f'cmake .. -DCMAKE_BUILD_TYPE={build_type} -DBUILD_SHARED_LIBS=OFF -DNFD_BUILD_TESTS=OFF')
                run_command('cmake --build . --config {0}'.format(build_type))
            
            # Copy Lib
            # Note: The output is usually in src/Release/nfd.lib
            lib_src = f"src/{build_type}/{LIB_PREFIX}nfd{LIB_EXT}"
            if not os.path.exists(lib_src):
                # Fallback for different CMake generator structures
                lib_src = f"{build_type}/{LIB_PREFIX}nfd{LIB_EXT}"
            
            if os.path.exists(lib_src):
                shutil.copy(lib_src, os.path.join(lib_output_dir, f"{LIB_PREFIX}nfd{LIB_EXT}"))
                if is_debug and LIB_EXT == ".lib":
                    pdb_src = lib_src.replace(".lib", ".pdb")
                    if os.path.exists(pdb_src):
                        shutil.copy(pdb_src, os.path.join(lib_output_dir, "nfd.pdb"))
            else:
                error_exit(f"Build successful but nfd{LIB_EXT} not found.")

            # Copy Headers
            nfd_inc_dir = os.path.join(libs_header_dir, "nfd-ext")
            src_inc = os.path.abspath(os.path.join("..", "src", "include"))
            
            # Use our copy_tree helper to copy the include folder contents
            if os.path.exists(src_inc):
                copy_tree(src_inc, nfd_inc_dir)
            else:
                log(f"[WARNING] Could not find NFD headers in {src_inc}")
            
            os.chdir(dependency_dir)

    # -------------------------------------------------------------------------
    # 10. nlohmann-json (Header-only)
    # -------------------------------------------------------------------------
    if feats["JSON"]:
        json_header_dir = os.path.join(libs_header_dir, "nlohmann")
        json_target = os.path.join(json_header_dir, "json.hpp")

        if update_all or not os.path.exists(json_target):
            log(f"Setting up nlohmann-json v{VERSIONS['JSON']}...")
            
            # Create destination directory
            if not os.path.exists(json_header_dir): os.makedirs(json_header_dir)
            
            # Download and extract
            dirname = f"json-{VERSIONS['JSON']}"
            if not os.path.exists(dirname):
                download_file(URLS["JSON"], "json.tar.gz")
                extract_archive("json.tar.gz", ".")
            
            # Copy the single header
            src_header = os.path.join(dirname, "single_include", "nlohmann", "json.hpp")
            if os.path.exists(src_header):
                shutil.copy(src_header, json_header_dir)
            else:
                log(f"[WARNING] Could not find json.hpp in {src_header}")

    # -------------------------------------------------------------------------
    # 11. stb_image (Header-only)
    # -------------------------------------------------------------------------
    if feats["STB"]:
        stb_header_dir = os.path.join(libs_header_dir, "stb_image")
        stb_target = os.path.join(stb_header_dir, "stb_image.h")

        if update_all or not os.path.exists(stb_target):
            log("Setting up stb_image...")
            
            if not os.path.exists(stb_header_dir): os.makedirs(stb_header_dir)
            
            # Direct download for single file
            download_file(URLS["STB_IMAGE"], stb_target)

    # -------------------------------------------------------------------------
    # 12. IconFontCppHeaders (Header-only)
    # -------------------------------------------------------------------------
    if feats["ICONS"]:
        icon_header_dir = os.path.join(libs_header_dir, "iconfonts")
        icon_target = os.path.join(icon_header_dir, "IconsFontAwesome6.h")

        if update_all or not os.path.exists(icon_target):
            log("Setting up IconFontCppHeaders...")
            
            if not os.path.exists(icon_header_dir): os.makedirs(icon_header_dir)
            
            # Download and extract
            if not os.path.exists("IconFontCppHeaders"):
                download_file(URLS["ICONFONT"], "IconFontCppHeaders.zip")
                extract_archive("IconFontCppHeaders.zip", ".")
                
                # Handle potential folder naming difference (e.g. -main suffix)
                if os.path.exists("IconFontCppHeaders-main"):
                    if os.path.exists("IconFontCppHeaders"):
                        shutil.rmtree("IconFontCppHeaders")
                    os.rename("IconFontCppHeaders-main", "IconFontCppHeaders")
            
            # Copy specific headers
            src_dir = "IconFontCppHeaders"
            files_to_copy = ["IconsFontAwesome6.h", "IconsFontAwesome6Brands.h"]
            
            for f in files_to_copy:
                src_file = os.path.join(src_dir, f)
                if os.path.exists(src_file):
                    shutil.copy(src_file, icon_header_dir)

    log("[SUCCESS] Done building dependencies...")

    log("\n=== Building Glimmer ===", BLUE)
    BUILD_DIR = os.path.join(PROJECT_ROOT, "build")
    
    if os.path.exists(BUILD_DIR) and args.clean:
        shutil.rmtree(BUILD_DIR)
    if not os.path.exists(BUILD_DIR): os.makedirs(BUILD_DIR)
    
    os.chdir(BUILD_DIR)
    
    # Configure
    cmake_cmd = ["cmake", "..", f"-DCMAKE_BUILD_TYPE={build_type}", f"-DGLIMMER_PLATFORM={args.platform}"]
    
    if sys.platform == "win32":
        cmake_cmd.extend(['-G', cmake_generator, '-A', 'x64', "--fresh" ])
    
    # Feature Flags
    if args.disable_svg: cmake_cmd.append("-DGLIMMER_DISABLE_SVG=ON")
    if args.disable_images: cmake_cmd.append("-DGLIMMER_DISABLE_IMAGES=ON")
    if args.disable_richtext: cmake_cmd.append("-DGLIMMER_DISABLE_RICHTEXT=ON")
    if args.disable_plots: cmake_cmd.append("-DGLIMMER_DISABLE_PLOTS=ON")
    if feats["NFD"]: cmake_cmd.append("-DGLIMMER_ENABLE_NFDEXT=ON")
    if feats["BLEND2D"]: cmake_cmd.append("-DGLIMMER_ENABLE_BLEND2D=ON")
    if args.update: cmake_cmd.append("-DGLIMMER_FORCE_UPDATE=ON")
    if args.clean: shutil.rmtree(os.path.join(BUILD_DIR, "..", "dependency"), ignore_errors=True)

    run_command(cmake_cmd)

    # Build
    build_cmd = ["cmake", "--build", ".", "--config", build_type]
    if sys.platform != "win32":
        build_cmd.extend(["--", f"-j{multiprocessing.cpu_count()}"])
    
    run_command(build_cmd)
    
    log(f"Glimmer ({build_type}) build complete!", GREEN)
    log(f"Output located in {BUILD_DIR}", YELLOW)
    if os.path.exists(os.path.join(BUILD_DIR, "..", "staticlib", "combined", build_type)) == False:
        os.makedirs(os.path.join(BUILD_DIR, "..", "staticlib", "combined", build_type))

    # Combine all libs into one glimmer.lib
    if sys.platform == 'win32':
        libs = glob.glob(os.path.join(lib_output_dir, "*.lib"))
        glimmer_lib = os.path.join(BUILD_DIR, "..", "staticlib", build_type, f"glimmer_{args.platform}.lib")
        if os.path.exists(glimmer_lib):
            libs.append(glimmer_lib)
            combined_lib = os.path.join(BUILD_DIR, "..", "staticlib", "combined", build_type, "glimmer.lib")
            cmd = ["lib.exe", "/OUT:" + combined_lib] + libs
            run_command(cmd, env=vc_env)
            log(f"Combined lib created: {combined_lib}", GREEN)
        else:
            log(f"Glimmer lib not found: {glimmer_lib}", YELLOW)
    else:
        # Linux: combine .a files using ar
        import tempfile
        libs = glob.glob(os.path.join(lib_output_dir, "*.a"))
        glimmer_lib = os.path.join(BUILD_DIR, "..", "staticlib", build_type, f"libglimmer_{args.platform}.a")
        if os.path.exists(glimmer_lib):
            libs.append(glimmer_lib)
            combined_lib = os.path.join(BUILD_DIR, "..", "staticlib", "combined", build_type, "libglimmer.a")
            with tempfile.TemporaryDirectory() as tmpdir:
                os.chdir(tmpdir)
                for lib in libs:
                    run_command(f"ar x {lib}")
                run_command("ar rcs libglimmer.a *.o")
                shutil.copy("libglimmer.a", combined_lib)
            log(f"Combined lib created: {combined_lib}", GREEN)
        else:
            log(f"Glimmer lib not found: {glimmer_lib}", YELLOW)

if __name__ == "__main__":
    main()
