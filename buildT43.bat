cd fastdoom
wmake fdoomtxt.exe EXTERNOPT=/dMODE_T8043 %1 %2 %3 %4 %5 %6 %7 %8 %9
copy fdoomtxt.exe ..\fdoomt43.exe
cd ..
sb -r fdoomt43.exe
ss fdoomt43.exe dos32a.d32