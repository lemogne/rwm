#!/usr/bin/sh
rm -f -- libdesktop.so rwm
separatelib=1
args=""
for i in "$@"
do
	if [ "$i" = "DEBUG" ]; then
		args="-O0 -g"
	elif [ "$i" = "GENNOLIB" ]; then
		separatelib=0
	fi
done

if [ separatelib = 1 ]; then
	g++ -shared -o libdesktop.so -fPIC desktop.cpp
	g++ $args rwm.cpp windows.cpp charencoding.cpp -o rwm -lncursesw -L. -ldesktop
else 
	g++ $args rwm.cpp windows.cpp desktop.cpp charencoding.cpp -o rwm -lncursesw
fi
