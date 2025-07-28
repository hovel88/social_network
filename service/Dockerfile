FROM alpine:3.19.7 AS builder

RUN apk update \
 && apk add build-base linux-headers util-linux-dev \
        openssl openssl-dev openssl-libs-static \
        curl curl-dev \
        spdlog spdlog-dev \
        libuv libuv-dev c-ares c-ares-dev c-ares-static \
        libpq libpq-dev \
        nlohmann-json \
        jsoncpp-dev jsoncpp-static \
        zlib zlib-dev zlib-static \
        brotli-dev brotli-static \
        ossp-uuid-dev ossp-uuid-libs \
 && apk add git cmake \
 && rm -rf /var/cache/apk/*

# LIB: libbcrypt
COPY <<EOF /install_libbcrypt.sh
#!/bin/sh
# set -eux

git clone https://github.com/trusch/libbcrypt
cd libbcrypt
mkdir build
cd build
cmake -DBUILD_SHARED_LIBS=OFF ..
make
make install
cd ../..
rm -rf ./libbcrypt/
EOF
RUN chmod u+x /install_libbcrypt.sh && ./install_libbcrypt.sh

# LIB: libpqxx v7.10.1
COPY <<EOF /install_libpqxx.sh
#!/bin/sh
# set -eux

git clone https://github.com/jtv/libpqxx.git
cd libpqxx
git checkout 7.10.1
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Release -DBUILD_TEST=OFF ..
cmake --build . --parallel 4
cmake --install .
cd ../..
rm -rf ./libpqxx/
EOF
RUN chmod u+x /install_libpqxx.sh && ./install_libpqxx.sh

# LIB: cpp-httplib v0.20.0
COPY <<EOF /install_cpp-httplib.sh
#!/bin/sh
# set -eux

git clone https://github.com/yhirose/cpp-httplib.git
cd cpp-httplib
git checkout v0.20.0
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Release -DHTTPLIB_TEST=OFF -DHTTPLIB_USE_CERTS_FROM_MACOSX_KEYCHAIN=OFF -DOPENSSL_USE_STATIC_LIBS=ON ..
cmake --build . --parallel 4
cmake --install .
cd ../..
rm -rf ./cpp-httplib/
EOF
RUN chmod u+x /install_cpp-httplib.sh && ./install_cpp-httplib.sh

# LIB: cxxopts v3.2.0
COPY <<EOF /install_cxxopts.sh
#!/bin/sh
# set -eux

git clone https://github.com/jarro2783/cxxopts.git
cd cxxopts
git checkout v3.2.0
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Release -DCXXOPTS_BUILD_TESTS=OFF -DCXXOPTS_BUILD_EXAMPLES=OFF ..
cmake --build . --parallel 4
cmake --install .
cd ../..
rm -rf ./cxxopts/
EOF
RUN chmod u+x /install_cxxopts.sh && ./install_cxxopts.sh

# LIB: prometheus-cpp v1.3.0
COPY <<EOF /install_prometheus-cpp.sh
#!/bin/sh
# set -eux

git clone https://github.com/jupp0r/prometheus-cpp.git
cd prometheus-cpp
git checkout v1.3.0
git submodule init
git submodule update
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=OFF -DENABLE_TESTING=OFF ..
cmake --build . --parallel 4
cmake --install .
cd ../..
rm -rf ./prometheus-cpp/
EOF
RUN chmod u+x /install_prometheus-cpp.sh && ./install_prometheus-cpp.sh

# LIB: caches v0.1.0
COPY <<EOF /install_caches.sh
#!/bin/sh
# set -eux

git clone https://github.com/vpetrigo/caches.git
cd caches
git checkout v0.1.0
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Release -DBUILD_TEST=OFF -DINSTALL_CACHES=ON ..
cmake --build . --parallel 4
cmake --install .
cd ../..
rm -rf ./caches/
EOF
RUN chmod u+x /install_caches.sh && ./install_caches.sh

# LIB: uvw v3.3.0_libuv_v1.47 (устанавливается libuv v1.47)
COPY <<EOF /install_uvw.sh
#!/bin/sh
# set -eux

git clone https://github.com/skypjack/uvw.git
cd uvw
git checkout v3.3.0_libuv_v1.47
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Release -DUVW_BUILD_TESTING=OFF -DUVW_BUILD_DOCS=OFF ..
cmake --build . --parallel 4
cmake --install .
cd ../..
rm -rf ./uvw/
EOF
RUN chmod u+x /install_uvw.sh && ./install_uvw.sh

# LIB: drogon v1.9.11
COPY <<EOF /install_drogon.sh
#!/bin/sh
# set -eux

git clone https://github.com/drogonframework/drogon.git
cd drogon
git checkout v1.9.11
git submodule init
git submodule update
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Release -DUSE_STATIC_LIBS_ONLY=ON -DBUILD_EXAMPLES=OFF -DBUILD_ORM=OFF -DBUILD_DOC=OFF -DBUILD_CTL=OFF -DUUID_LIBRARIES=/usr/lib/libossp-uuid.so -DUUID_INCLUDE_DIRS=/usr/include ..
cmake --build . --parallel 4
cmake --install .
cd ../..
rm -rf ./drogon/
EOF
RUN chmod u+x /install_drogon.sh && ./install_drogon.sh

# LIB: uWebSockets v20.74.0
COPY <<EOF /install_uWebSockets.sh
#!/bin/sh
# set -eux

git clone https://github.com/uNetworking/uWebSockets.git
cd uWebSockets
git checkout v20.74.0
# git submodule init
# git submodule update
# WITH_OPENSSL=1 WITH_ZLIB=1 WITH_LIBUV=1 make
make install
cd ..
rm -rf ./uWebSockets/
EOF
RUN chmod u+x /install_uWebSockets.sh && ./install_uWebSockets.sh

WORKDIR /app-src
COPY . /app-src

RUN ./make_script.sh

# -----------------------------------------------------------------------------

FROM alpine:3.19.7

RUN apk update \
 && apk add libstdc++ libpq ossp-uuid-libs \
 && rm -rf /var/cache/apk/*

WORKDIR /service
COPY --from=builder /app-src/_build/bin/social_network /service/
ENTRYPOINT ["./social_network"]
