#!/bin/bash

set -eux

CURRENT_DIR=$(cd "$(dirname "$BASH_SOURCE")"; pwd)
GLUTEN_DIR="$CURRENT_DIR/.."
THIRDPARTY_LIB="$GLUTEN_DIR/package/target/thirdparty-lib"
LINUX_OS=$(. /etc/os-release && echo ${ID})
VERSION=$(. /etc/os-release && echo ${VERSION_ID})

# compile gluten jar
$GLUTEN_DIR/dev/builddeps-veloxbe.sh --build_tests=ON --build_benchmarks=ON --enable_s3=ON  --enable_hdfs=ON
mvn clean package -Pbackends-velox -Pspark-3.2 -DskipTests
mvn clean package -Pbackends-velox -Pspark-3.3 -DskipTests

mkdir -p $THIRDPARTY_LIB
function process_setup_ubuntu_2004 {
  cp /usr/lib/x86_64-linux-gnu/{libroken.so.18,libasn1.so.8,libboost_context.so.1.71.0,libboost_regex.so.1.71.0,libcrypto.so.1.1,libnghttp2.so.14,libnettle.so.7,libhogweed.so.5,librtmp.so.1,libssh.so.4,libssl.so.1.1,liblber-2.4.so.2,libsasl2.so.2,libwind.so.0,libheimbase.so.1,libhcrypto.so.4,libhx509.so.5,libkrb5.so.26,libheimntlm.so.0,libgssapi.so.3,libldap_r-2.4.so.2,libcurl.so.4,libdouble-conversion.so.3,libevent-2.1.so.7,libgflags.so.2.2,libunwind.so.8,libglog.so.0,libidn.so.11,libntlm.so.0,libgsasl.so.7,libicudata.so.66,libicuuc.so.66,libxml2.so.2,libre2.so.5,libsnappy.so.1,libpsl.so.5,libbrotlidec.so.1,libbrotlicommon.so.1} $THIRDPARTY_LIB/
  cp /usr/local/lib/{libprotobuf.so.32,libhdfs3.so.1} $THIRDPARTY_LIB/
}

function process_setup_ubuntu_2204 {
  cp /usr/lib/x86_64-linux-gnu/{libre2.so.9,libboost_context.so.1.74.0,libboost_regex.so.1.74.0,libdouble-conversion.so.3,libidn.so.12,libglog.so.0,libgflags.so.2.2,libevent-2.1.so.7,libsnappy.so.1,libunwind.so.8,libcurl.so.4,libxml2.so.2,libgsasl.so.7,libicui18n.so.70,libicuuc.so.70,libnghttp2.so.14,libldap-2.5.so.0,liblber-2.5.so.0,libntlm.so.0,librtmp.so.1,libsasl2.so.2,libssh.so.4,libicudata.so.70} $THIRDPARTY_LIB/
  cp /usr/local/lib/{libhdfs3.so.1,libprotobuf.so.32} $THIRDPARTY_LIB/
}

function process_setup_centos_8 {
  cp /usr/lib64/{libre2.so.0,libdouble-conversion.so.3,libgflags.so.2.2,libglog.so.0,libevent-2.1.so.6,libdwarf.so.1,libgsasl.so.7,libicudata.so.60,libicui18n.so.60,libicuuc.so.60,libidn.so.11,libntlm.so.0} $THIRDPARTY_LIB/
  cp /usr/local/lib/{libhdfs3.so.1,libboost_context.so.1.72.0,libboost_filesystem.so.1.72.0,libboost_program_options.so.1.72.0,libboost_regex.so.1.72.0,libboost_system.so.1.72.0,libboost_thread.so.1.72.0,libprotobuf.so.32} $THIRDPARTY_LIB/
}

function process_setup_centos_7 {
  cp /usr/local/lib64/{libgflags.so.2.2,libglog.so.0} $THIRDPARTY_LIB/
  cp /usr/lib64/{libdouble-conversion.so.1,libevent-2.0.so.5,libzstd.so.1,libntlm.so.0,libgsasl.so.7} $THIRDPARTY_LIB/
  cp /usr/local/lib/{libre2.so.10,libhdfs3.so.1,libboost_context.so.1.72.0,libboost_filesystem.so.1.72.0,libboost_program_options.so.1.72.0,libboost_system.so.1.72.0,libboost_thread.so.1.72.0,libboost_regex.so.1.72.0,libprotobuf.so.32} $THIRDPARTY_LIB/
}

if [ "$LINUX_OS" == "ubuntu" ]; then
  if [ "$VERSION" == "20.04" ]; then
    process_setup_ubuntu_2004
  elif [ "$VERSION" == "22.04" ]; then
    process_setup_ubuntu_2204
  fi
elif [ "$LINUX_OS" == "centos" ]; then
  if [ "$VERSION" == "8" ]; then
    process_setup_centos_8
  elif [ "$VERSION" == "7" ]; then
    process_setup_centos_7
  fi
elif [ "$LINUX_OS" == "alinux" ]; then
  if [ "$VERSION" == "3" ]; then
    process_setup_centos_8
  fi
fi
cd $THIRDPARTY_LIB/
jar cvf gluten-thirdparty-lib-$LINUX_OS-$VERSION.jar ./
