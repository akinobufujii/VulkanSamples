#version 400

#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

layout (binding = 0) uniform MATRICES 
{
	mat4 projectionMatrix;
	mat4 viewMatrix;
	mat4 worldMatrix;
} matrices;

layout (location = 0) in vec3 inPos;
layout (location = 1) in vec2 inUV;
layout (location = 2) in vec3 inNormal;
layout (location = 3) in vec4 inColor;
layout (location = 0) out vec4 outColor;
layout (location = 1) out vec2 outUV;

void main() 
{
	outColor = inColor;
	outUV = inUV;
	gl_Position = matrices.projectionMatrix * matrices.viewMatrix * matrices.worldMatrix * vec4(inPos.xyz, 1.0);
}
