mkdir src
rsync -v 192.168.0.4::visftp/Makefile Makefile
rsync -v 192.168.0.4::visftp/src/*.c src
rsync -v 192.168.0.4::visftp/src/*.h src

mkdir libs
rsync -rv 192.168.0.4::visftp/libs/watt32 libs

mkdir scripts
rsync -v 192.168.0.4::visftp/scripts/* scripts

mkdir doc
rsync -v 192.168.0.4::visftp/doc/* doc
