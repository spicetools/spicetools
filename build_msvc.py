from typing import Dict, List

import argparse
import os
import subprocess


def get_vs_installation_path() -> str:
    program_files_x86 = os.environ["ProgramFiles(x86)"]
    process = subprocess.run(
        [f"{program_files_x86}\\Microsoft Visual Studio\\Installer\\vswhere.exe", "-prerelease", "-latest", "-property",
         "installationPath"],
        capture_output=True, check=True, encoding="utf-8")

    return process.stdout.strip()


def source_bat(bat_file: str, arch: str) -> Dict[str, str]:
    interesting = {"INCLUDE", "LIB", "LIBPATH", "PATH"}
    result = {}

    process = subprocess.Popen(f"\"{bat_file}\" {arch} & set", stdout=subprocess.PIPE, shell=True, encoding="utf-8")
    (out, err) = process.communicate()

    if err is not None:
        raise Exception(err)

    for line in out.split("\n"):
        if '=' not in line:
            continue

        key, value = line.strip().split('=', 1)
        key = key.upper()

        if key in interesting:
            result[key] = value

    return result


def run_build(build_dir: str, build_type: str, build_targets: List[str], build_env: Dict[str, str]):
    os.makedirs(build_dir, exist_ok=True)

    subprocess.check_call(["cmake.exe", f"-DCMAKE_BUILD_TYPE={build_type}", "-GNinja", ".."], cwd=build_dir,
                          env=build_env, shell=True)
    subprocess.check_call(["ninja.exe"] + build_targets, cwd=build_dir, env=build_env, shell=True)


def main():
    parser = argparse.ArgumentParser(description='Build SpiceTools with MSVC')
    parser.add_argument('--build-type', type=str, default='Release', help='CMake build type')

    args = parser.parse_args()

    build_dir = args.build_type.lower()
    parent_env = {key: os.environ[key] for key in os.environ}

    vs_installation_path = get_vs_installation_path()
    bat_file = f"{vs_installation_path}\\VC\\Auxiliary\\Build\\vcvarsall.bat"
    print(bat_file)

    # parent_env["CC"] = "clang"
    # parent_env["CXX"] = "clang++"

    print("Building spice64")

    env_64 = source_bat(bat_file, "x64")
    env_64_merged = parent_env.copy()
    env_64_merged.update(env_64)

    run_build(f"cmake-build-{build_dir}-64", args.build_type,
              ["spicetools_stubs_kbt64", "spicetools_stubs_kld64", "spicetools_spice64"], env_64_merged)

    print("Building spice")

    env_32 = source_bat(bat_file, "x86")
    env_32_merged = parent_env.copy()
    env_32_merged.update(env_32)

    run_build(f"cmake-build-{build_dir}-32", args.build_type,
              ["spicetools_stubs_kbt", "spicetools_stubs_kld", "spicetools_cfg", "spicetools_spice"], env_32_merged)


if __name__ == "__main__":
    main()
