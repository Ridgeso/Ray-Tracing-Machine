###SHADER VERTEX
#version 450 core

layout(location = 0) in vec2 position;
layout(location = 1) in vec3 color;

layout(location = 0) out vec3 fragColor;

void main()
{
    gl_Position = vec4(position, 0.0, 1.0);
    fragColor = color;
}

###SHADER FRAGMENT
#version 450 core

layout(location = 0) in vec3 inColor;

layout(location = 0) out vec4 outColor0;
layout(location = 1) out vec4 outColor1;

layout(set = 0, binding = 0)
uniform myBuff
{
    vec4 color;
};

layout(set = 0, binding = 1)
uniform sampler2D tex;

struct ObjectData
{
    vec4 anotherColor;
    mat4 model;
};

layout(std140, set = 0, binding = 2)
readonly buffer bufferObjectBuffer
{
    ObjectData objects[];
};

void main()
{
    //outColor0 = vec4(inColor, 1.0);
    //outColor1 = vec4(inColor, 1.0);
    
    //outColor0 = color;
    //outColor1 = color;

    outColor0 = objects[0].anotherColor;
    outColor1 = objects[0].anotherColor;
}
