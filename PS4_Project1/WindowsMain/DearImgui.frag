// Vertex Shader - file "2Dpass.frag"

#version 420

precision highp float; // needed only for version 1.30

in  vec2 ex_TexCoord;
in  vec4 ex_Color;
out vec4 out_Color;

layout(binding = 0) uniform sampler2D albedoTexture;

void main(void)
{
	vec4 texColor = texture(albedoTexture, ex_TexCoord);
	out_Color = texColor * ex_Color;
}