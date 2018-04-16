
#version 420

layout (std140, binding = 0) uniform InstanceData // read from the buffer slot 0
{
	mat4 modelMatrix;
	vec4 colorModifier;
};


layout(location = 0) in  vec3 in_Position;
layout(location = 1) in  vec4 in_Color;
layout(location = 2) in  vec2 in_TexCoord;

out vec2 ex_TexCoord;
out vec4 ex_Color;

void main(void)
{
	gl_Position =  modelMatrix * vec4(in_Position, 1.0);
	ex_Color = in_Color;
	ex_TexCoord = vec2(in_TexCoord.s, 1.0 - in_TexCoord.t);
}
