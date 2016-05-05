echo off
echo 頂点シェーダコンパイル
glslangvalidator -V triangle.vert -o triangle.vert.spv

echo フラグメントシェーダコンパイル
glslangvalidator -V triangle.frag -o triangle.frag.spv

echo コンパイル完了
pause
echo on
