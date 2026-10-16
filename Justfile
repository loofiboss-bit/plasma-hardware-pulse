set shell := ["bash", "-cu"]

default: build test

configure:
    cmake -B build -DBUILD_TESTING=ON -DCMAKE_BUILD_TYPE=RelWithDebInfo

build: configure
    cmake --build build -j"$(nproc)"

test: build
    ctest --test-dir build --output-on-failure

run-app: build
    QML_IMPORT_PATH="{{justfile_directory()}}/build/bin" ./build/bin/loofi-pulse

smoke: build
    QML_IMPORT_PATH="{{justfile_directory()}}/build/bin" ./build/bin/loofi-pulse --smoke-test

install-local: build
    cmake --install build --prefix "$HOME/.local"
    kpackagetool6 --type Plasma/Applet --upgrade package || kpackagetool6 --type Plasma/Applet --install package

clean:
    rm -rf build
