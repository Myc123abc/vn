cmake -Bbuild -GNinja .
cmake --build build

md .\build\assets
copy .\assets\* .\build\assets\