# FROM ubuntu:noble AS builder

# RUN apt-get update && \
#   apt-get install -y git g++ cmake \
#     nlohmann-json3-dev \
#     libpqxx-dev && \
#   apt-get clean && \
#   rm -rf /var/lib/apt/lists/*

# COPY <<EOF /install_libbcrypt.sh
# #!/bin/bash
# # set -eux

# git clone https://github.com/trusch/libbcrypt
# cd libbcrypt
# mkdir build
# cd build
# cmake ..
# make
# make install
# cd ../..
# rm -rf ./libbcrypt/
# EOF
# RUN chmod u+x /install_libbcrypt.sh && ./install_libbcrypt.sh

# COPY . /app-src
# WORKDIR /app-src
# RUN g++ -std=c++20 -O3 -DNDEBUG -Iinclude -lpqxx -lpq -lbcrypt -o app sources/configuration/*.cpp sources/helpers/*.cpp sources/logger/*.cpp sources/thread_pool/*.cpp sources/*.cpp

# -----------------------------------------------------------------------------

# FROM ubuntu:noble
# COPY --from=builder /app-src/app /service/
# WORKDIR /service
# CMD ["./app"]


# -----------------------------------------------------------------------------

FROM alpine:3.19.7 AS builder

RUN apk update \
 && apk add build-base linux-headers \
        openssl openssl-dev openssl-libs-static \
        curl curl-dev \
        libev libev-dev c-ares c-ares-dev c-ares-static \
        libpq libpq-dev \
        git nlohmann-json \
        util-linux-dev \
 && apk add cmake \
 && rm -rf /var/cache/apk/*

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

WORKDIR /app-src
COPY . /app-src

RUN ./make_script.sh

# -----------------------------------------------------------------------------

FROM alpine:3.19.7

RUN apk update \
 && apk add libstdc++ libpq libuuid \
 && rm -rf /var/cache/apk/*

WORKDIR /service
COPY --from=builder /app-src/_build/bin/social_network /service/
ENTRYPOINT ["./social_network"]
