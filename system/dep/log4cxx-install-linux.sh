#!/bin/bash

wget http://apache.spinellicreations.com//apr/apr-1.4.6.tar.gz
tar -xvf apr-1.4.6.tar.gz
cd dep/apr-1.4.6/
./configure
make install
cd ..

wget http://apache.spinellicreations.com//apr/apr-util-1.5.1.tar.gz
tar -xvf apr-util-1.5.1.tar.gz
cd apr-util-1.5.1/
./configure --with-apr=/usr/local/apr
make install
cd ..

unzip apache-log4cxx-0.10.0.zip
cd apache-log4cxx-0.10.0/
./configure --with-apr=/usr/local/apr
cp ../patch* .
cp ../*patch .
./patch-log4cxx.sh
make install
