#!/usr/bin/python3

# Adds PlatformIO post-processing to merge all the ESP flash images into a single image.

import os

Import("env", "projenv")

board_config = env.BoardConfig()
firmware_bin = "${BUILD_DIR}/${PROGNAME}.bin"
merged_bin = os.environ.get("MERGED_BIN_PATH", "${BUILD_DIR}/${PROGNAME}-merged.bin")


def merge_bin_action(source, target, env):
    flash_images = [
        *env.Flatten(env.get("FLASH_EXTRA_IMAGES", [])),
        "$ESP32_APP_OFFSET",
        source[0].get_abspath(),
    ]
    actual_images = [env.subst(str(x)) for x in flash_images]
    quoted_images = [f'"{x}"' if " " in x and not x.startswith('"') else x for x in actual_images]
    actual_merged_bin = env.subst(merged_bin)
    quoted_merged_bin = f'"{actual_merged_bin}"'
    merge_cmd = " ".join(
        [
            '"$PYTHONEXE"',
            '"$OBJCOPY"',
            "--chip",
            board_config.get("build.mcu", "esp32"),
            "merge-bin",
            "-o",
            quoted_merged_bin,
            "--flash-mode",
            board_config.get("build.flash_mode", "dio"),
            "--flash-freq",
            "${__get_board_f_flash(__env__)}",
            "--flash-size",
            board_config.get("upload.flash_size", "4MB"),
            *quoted_images,
        ]
    )
    env.Execute(merge_cmd)


env.AddCustomTarget(
    name="mergebin",
    dependencies=firmware_bin,
    actions=merge_bin_action,
    title="Merge binary",
    description="Build combined image",
    always_build=True,
)