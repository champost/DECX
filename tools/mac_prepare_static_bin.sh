dd=`echo $RANDOM`
dir="build_$dd"
mkdir $dir
cp $1 $dir/
cd $dir

mkdir lib
#cp /sw/lib/gcc4.9/lib/libgfortran.3.dylib /sw/lib/gcc4.9/lib/libquadmath.0.dylib /sw/lib/libgsl.0.dylib /sw/lib/libgslcblas.0.dylib /sw/lib/gcc4.9/lib/libstdc++.6.dylib /usr/lib/libSystem.B.dylib /sw/lib/gcc4.9/lib/libgcc_s.1.dylib ./lib/
for target in `otool -L lagrange_cpp | awk '{if (NF > 1) {if ($1 !~ /^@/) print $1} }'`; do
    cp $target ./lib/
done


echo
echo change libs of executable
echo
for target in `otool -L lagrange_cpp | awk '{if (NF > 1) {if ($1 !~ /^@/) print $1} }'`; do
	targetname=`echo $target | awk -F '/' '{print $NF}'`
	echo install_name_tool -change $target @executable_path/lib/$targetname lagrange_cpp
	install_name_tool -change $target @executable_path/lib/$targetname lagrange_cpp
done

# FOR EXAMPLE, DOES THAT :
#install_name_tool -change /sw/lib/gcc4.9/lib/libquadmath.0.dylib @executable_path/lib/libquadmath.0.dylib lagrange_cpp

echo
echo change ids of libs
echo
for lib in ./lib/*.dylib; do
	name=`echo $lib | awk -F '/' '{print $NF}'`
	echo install_name_tool -id @executable_path/lib/$name $lib
	install_name_tool -id @executable_path/lib/$name $lib
done
# FOR EXAMPLE, DOES THAT :
#install_name_tool -id @executable_path/lib/libquadmath.0.dylib ./lib/libquadmath.0.dylib

# change internal links of libs
echo
echo change internal links of libs
echo
cd lib/
#install_name_tool -change /usr/lib/libSystem.B.dylib @executable_path/lib/libSystem.B.dylib
for lib in `ls *.dylib | grep -v libSystem`; do
	targets=`otool -L $lib | awk '{if (NF > 1) {if ($1 !~ /^@/) print $1} }'`
	for target in $targets; do
		targetname=`echo $target | awk -F '/' '{print $NF}'`
		echo install_name_tool -change $target @executable_path/lib/$targetname $lib
		install_name_tool -change $target @executable_path/lib/$targetname $lib
	done	
done

cd ../../

#mkdir decx_macos_static
#cp -r lib lagrange_cpp decx_macos_static
#tar cvf decx_macos_static.tar decx_macos_static
#rm -r decx_macos_static

echo $dir
