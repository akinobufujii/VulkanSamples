echo off
echo 頂点シェーダコンパイル
glslangvalidator -V meshUseTexture.vert -o meshUseTexture.vert.spv

echo フラグメントシェーダコンパイル
glslangvalidator -V meshUseTexture.frag -o meshUseTexture.frag.spv

echo コンパイル完了
pause
echo on
