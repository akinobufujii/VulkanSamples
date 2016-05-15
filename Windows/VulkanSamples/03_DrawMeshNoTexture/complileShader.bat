echo off
echo 頂点シェーダコンパイル
glslangvalidator -V mesh.vert -o mesh.vert.spv

echo フラグメントシェーダコンパイル
glslangvalidator -V mesh.frag -o mesh.frag.spv

echo コンパイル完了
pause
echo on
