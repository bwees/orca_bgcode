# orca_bgcode

Binary G-Code Post Processor for OrcaSlicer

## Installation

1. Get bgcode binary

    - (recommended) Download the correct binary for your machine from the [latest release](https://github.com/bwees/orca_bgcode/releases/latest)
    
    - Or clone the repository and run the following to compile:

        ```
        cmake --preset default -DLibBGCode_BUILD_DEPS=ON -DCMAKE_INSTALL_PREFIX=dist/  
        cmake --build --preset default --target install
        ```


2. In OrcaSlicer, add the following to your post-processor steps:

    ```<path to orca_bgcode> --post-processor --slicer_metadata_compression=1 --gcode_compression=3 --gcode_encoding=2```
