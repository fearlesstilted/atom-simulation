#version 330

in vec3 fragPosition;
in vec3 fragNormal;

uniform vec3 viewPos;
uniform vec3 phaseColor;

out vec4 finalColor;

void main()
{
    vec3 normal = normalize(fragNormal);
    vec3 lightDirection = normalize(vec3(0.4, 0.8, 0.3));
    vec3 viewDirection = normalize(viewPos - fragPosition);

    float diffuse = max(dot(normal, lightDirection), 0.0);
    float specular = pow(max(dot(reflect(-lightDirection, normal),
                                 viewDirection), 0.0), 24.0);
    vec3 color = phaseColor * (0.35 + 1.15 * diffuse);
    color += vec3(specular * 1.5);
    finalColor = vec4(color, 1.0);
}
