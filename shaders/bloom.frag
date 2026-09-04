#version 330

in vec2 fragTexCoord;
in vec4 fragColor;

uniform sampler2D texture0;
uniform vec2 resolution;

out vec4 finalColor;

vec3 brightSample(vec2 uv)
{
    vec3 color = texture(texture0, uv).rgb;
    float brightness = max(color.r, max(color.g, color.b));
    return color * smoothstep(0.45, 1.0, brightness);
}

void main()
{
    vec2 texel = 1.0 / resolution;
    vec3 base = texture(texture0, fragTexCoord).rgb;
    vec3 glow = brightSample(fragTexCoord + vec2( 2.0,  0.0) * texel);
    glow += brightSample(fragTexCoord + vec2(-2.0,  0.0) * texel);
    glow += brightSample(fragTexCoord + vec2( 0.0,  2.0) * texel);
    glow += brightSample(fragTexCoord + vec2( 0.0, -2.0) * texel);
    glow += brightSample(fragTexCoord + vec2( 4.0,  4.0) * texel) * 0.5;
    glow += brightSample(fragTexCoord + vec2(-4.0,  4.0) * texel) * 0.5;
    glow += brightSample(fragTexCoord + vec2( 4.0, -4.0) * texel) * 0.5;
    glow += brightSample(fragTexCoord + vec2(-4.0, -4.0) * texel) * 0.5;
    finalColor = vec4(base + glow * 0.07, 1.0) * fragColor;
}
