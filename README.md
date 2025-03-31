# OrcaSlicer Binary G-Code (bgcode) Support

This allows OrcaSlicer to output bgcode for use with Prusa Machines through a simple post-processing script that can be added to any print profile. Also supports renaming the output file to have the `.bgcode` extension.

## Installation

1. Get bgcode binary

    - (recommended) Download the correct binary for your machine from the [latest release](https://github.com/bwees/orca_bgcode/releases/latest)
    
    - Or clone the repository and run the following to compile:

        ```
        cmake --preset default -DLibBGCode_BUILD_DEPS=ON -DCMAKE_INSTALL_PREFIX=dist/  
        cmake --build --preset default --target install
        ```


2. In OrcaSlicer, add the following to your post-processor steps:

    ```
    <path to bgcode binary> --post-processor --slicer_metadata_compression=1 --gcode_compression=3 --gcode_encoding=2
    ```
