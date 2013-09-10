#!/bin/sh 

OPWD=`pwd`

CEPH_REPO='git://github.com/linuxbox2/linuxbox-ceph.git'

# remove ceph if present;  try to avoid making
# a mess
if [ -d src -a -d contrib ]; then
    if [ -e linuxbox-ceph ]; then
	rm -rf linuxbox-ceph
    fi
fi

git clone ${CEPH_REPO} linuxbox-ceph
cd linuxbox-ceph
git checkout master
git submodule update --init
./autogen.sh
./configure --prefix=/usr/local
make
sudo make install

cd ${OPWD}
