#!/usr/bin/sh
rm -f -- libdesktop.so rwm
separatelib=0
args="-O3"
for i in "$@"
do
	if [ "$i" = "DEBUG" ]; then
		args="-O0 -g"
	elif [ "$i" = "GENLIB" ]; then
		separatelib=1
	fi
done

(
	cd ./source || exit 1

	if [ $separatelib = 1 ]; then
		g++ --std=c++17 -shared -o ../libdesktop.so -fPIC desktop.cpp 
		g++ --std=c++17 $args rwm.cpp windows.cpp charencoding.cpp -o ../rwm -lncursesw -L. -ldesktop -lutil
	else 
		g++ --std=c++17 $args rwm.cpp windows.cpp desktop.cpp charencoding.cpp -o ../rwm -lncursesw -lutil
	fi
)