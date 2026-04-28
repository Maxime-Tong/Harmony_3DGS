Remove-Item build -Recurse -Force

mkdir build
cd build

cmake .. -DCMAKE_BUILD_TYPE=Debug
cmake --build .