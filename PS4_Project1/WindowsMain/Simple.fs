#version 420

// this come from the vertex shader
in  vec4 ex_Color;
in  vec2 ex_TexCoord;

layout(binding = 0) uniform sampler2D albedoTexture; // read from the texture slot 0

layout(location = 0) out vec4 out_Color; // write to the back buffer 0

layout (std140, binding = 0) uniform InstanceData // has to be equal to the one in the vertex shader
{
	mat4 modelMatrix;
	vec4 colorModifier;
};

void main(void)
{
	vec4 texColor = texture(albedoTexture, ex_TexCoord); // gets the texture "albedoTexture"'s color on the coordinates "ex_TexCoord"
	
	out_Color = ex_Color * texColor * colorModifier;
}