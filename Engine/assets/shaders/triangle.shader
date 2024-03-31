###SHADER VERTEX
#version 450

layout(location = 0) in vec2 position;
layout(location = 1) in vec3 color;

layout(location = 0) out vec3 fragColor;

void main()
{
    gl_Position = vec4(position, 0.0, 1.0);
    fragColor = color;
}

###SHADER FRAGMENT
#version 450

layout (location = 0) in vec3 inColor;

layout (location = 0) out vec4 outColor;

void main()
{
    outColor = vec4(inColor, 1.0);
}
