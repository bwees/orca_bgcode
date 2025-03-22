build:
    cmake --preset default -DLibBGCode_BUILD_DEPS=ON -DCMAKE_INSTALL_PREFIX=dist/  
    cmake --build --preset default --target install