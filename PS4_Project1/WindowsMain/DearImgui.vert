// Vertex Shader - file "2Dpass.vert"

#version 420

layout (std140, binding = 0) uniform ShaderData
{
	vec2 displaySize;
};

layout(location = 0) in  vec3 in_Position;
layout(location = 1) in  vec2 in_TexCoord;
layout(location = 2) in  vec4 in_Color;
out vec2 ex_TexCoord;
out vec4 ex_Color;

void main(void)
{
	gl_Position = vec4((in_Position.x / (0.5 * displaySize.x)) - 1.0, 1.0 - in_Position.y / (0.5 * displaySize.y), 0.0, 1.0);
	ex_TexCoord = in_TexCoord;
	ex_Color = in_Color;
}