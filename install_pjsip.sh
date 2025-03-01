apt-get install subversion build-essential automake autoconf libtool libasound2-dev libpulse-dev libssl-dev libsamplerate0-dev libcommoncpp2-dev libccrtp-dev libzrtpcpp-dev libdbus-1-dev libdbus-c++-dev libyaml-dev libpcre3-dev libgsm1-dev libspeex-dev libspeexdsp-dev libcelt-dev espeak espeak-data pkg-config make


# sudo apt-get install libasound2-dev binutils make g++ python2.7-dev swig
wget https://github.com/pjsip/pjproject/archive/refs/tags/2.15.1.tar.gz -O pjproject.tar.gz
tar -xvf pjproject.tar.gz
cd pjproject-2.15.1
opts="--disable-floating-point --disable-speex-aec --disable-large-filter"
codecs="--disable-gsm-codec --disable-speex-codec --disable-l16-codec --disable-ilbc-codec --disable-g722-codec --disable-g7221-codec"
./configure $opts $codecs && make dep && make clean && make
sudo make install

# then we run make on parrent directory
