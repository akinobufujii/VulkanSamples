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
layout (location = 1) in vec3 inColor;
layout (location = 0) out vec3 outColor;

void main() 
{
	outColor = inColor;
	gl_Position = matrices.projectionMatrix * matrices.viewMatrix * matrices.worldMatrix * vec4(inPos.xyz, 1.0);
}
