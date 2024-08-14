###SHADER VERTEX
#version 450 core

layout (location = 0) in vec2 aPos;
layout (location = 1) in vec2 aTexCoords;

layout (location = 0) out vec2 TexCoords;

void main()
{
    gl_Position = vec4(aPos, 0.0, 1.0);
    TexCoords = aTexCoords;
}

###SHADER FRAGMENT
#version 450 core

#define LOWETS_THRESHOLD 1.0e-6F
#define FLT_MAX 3.402823466e+38F
#define UINT_MAX 4294967295.0
#define PI 3.141592653589793

layout (location = 0) in vec2 TexCoords;

layout (location = 0) out vec4 AccumulationColor;
layout (location = 1) out vec4 ScreenColor;

layout(set = 0, binding = 0) uniform sampler2D AccumulationTexture;

layout(std140, set = 0, binding = 1) uniform Amounts
{
    float DrawEnvironment;
    uint MaxBounces;
    uint MaxFrames;
    uint FrameIndex;
    vec2 Resolution;
    int MaterialsCount;
    int SpheresCount;
};

layout(std140, set = 0, binding = 2) uniform CameraBuffer
{
    mat4 projection;
    mat4 view;
    vec3 position;
} Camera;

struct Material
{
    vec3 Albedo;
    vec3 EmmisionColor;
    float Roughness;
    float Metalic;
    float EmmisionPower;
    float RefractionRatio;
};

struct Sphere
{
    vec3 Position;
    float Radius;
    int MaterialId;
};

layout(std140, set = 1, binding = 0) readonly buffer MaterialsBuffer
{
    Material Materials[];
};

layout(std140, set = 1, binding = 1) readonly buffer SpheresBuffer
{
    Sphere Spheres[];
};

uint PCGhash(in uint random)
{
    uint state = random * 747796405u + 2891336453u;
    uint word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
    return (word >> 22u) ^ word;
}

float fastRandom(inout uint seed)
{
    seed = PCGhash(seed);
    return float(seed) / UINT_MAX;
}

vec3 randomUnitSpehere(inout uint seed)
{
    return 2.0 * vec3(fastRandom(seed), fastRandom(seed), fastRandom(seed)) - 1.0;
}

struct
{
    uint seed;
} Global;

struct Ray
{
    vec3 Origin;
    vec3 Direction;
};

struct Payload
{
    vec3 Color;
    vec3 HitPosition;
    vec3 HitNormal;
    float HitDistance;
    int HitObject;
};

struct Pixel
{
    vec3 Color;
    vec3 Contribution;
};

const vec3 LightDir = normalize(vec3(-1, -1, -1));
const vec3 GroundColor = vec3(0.3);
const vec3 SkyColorZenith = vec3(0.5, 0.7, 1.0);
const vec3 SkyColorHorizon = vec3(0.6, 0.4, 0.4);

vec3 getEmmision(in int matIndex)
{
    return Materials[matIndex].EmmisionColor * Materials[matIndex].EmmisionPower;
}

Payload miss(in Ray ray)
{
    //vec3 skyColor = vec3((1.0 - ray.Direction.yy) / 4.0 + 0.6, 1.0);
    float skyLerp = pow(smoothstep(0.0, 0.4, ray.Direction.y), 0.35);
    float groundToSky = smoothstep(-0.01, 0.0, ray.Direction.y);
    vec3 skyGradient = mix(SkyColorHorizon, SkyColorZenith, skyLerp);
    float sun = pow(max(0.0, dot(ray.Direction, -LightDir)), 500.0) * 100.0;
    vec3 skyColor = mix(GroundColor, skyGradient, groundToSky) + sun * float(groundToSky >= 1.0);
    
    Payload payload;
    payload.Color = skyColor;
    payload.HitPosition = vec3(0);
    payload.HitNormal = vec3(0);
    payload.HitDistance = FLT_MAX;
    payload.HitObject = -1;
    
    return payload;
}

Payload closestHit(in Ray ray, in float closestDistance, in int closestObject)
{   
    Payload payload;
    payload.Color = Materials[Spheres[closestObject].MaterialId].Albedo;
    payload.HitPosition = ray.Origin + closestDistance * ray.Direction;
    payload.HitNormal = normalize(payload.HitPosition - Spheres[closestObject].Position);
    payload.HitDistance = closestDistance;
    payload.HitObject = closestObject;
        
    return payload;
}

Payload traceRay(in Ray ray)
{
    float closestDistance = FLT_MAX;
    int closestObject = -1;
    
    for (int sphereId = 0; sphereId < SpheresCount; sphereId++)
    {
        vec3 origin = ray.Origin - Spheres[sphereId].Position;
        
        float a = dot(ray.Direction, ray.Direction);
        float b = 2.0 * dot(origin, ray.Direction);
        float c = dot(origin, origin) - pow(Spheres[sphereId].Radius, 2.0);
    
        float delta = b * b - 4.0 * a * c;
    
        if (delta < 0.0)
            continue;
        
        float sqrtDelta = sqrt(delta);
        
        float closestT = (-b - sqrtDelta) / (2.0 * a);
        if (closestT >= 0.0 && closestT < closestDistance)
        {
            closestDistance = closestT;
            closestObject = sphereId;
            continue;
        }
    }
    
    if (closestObject == -1)
        return miss(ray);
    
    return closestHit(ray, closestDistance, closestObject);
}

void accumulateColor(inout Pixel pixel, in int HitObject)
{
    pixel.Color += getEmmision(Spheres[HitObject].MaterialId) * pixel.Contribution;
    pixel.Contribution *= Materials[Spheres[HitObject].MaterialId].Albedo;
}

bool reflectance(in vec3 direction, in vec3 surfaceNormal, in float refIdx)
{
    float cosTheta = min(dot(-direction, surfaceNormal), 1.0);
    float sinTheta = sqrt(1.0 - cosTheta * cosTheta);
 
    bool cannotRefract = refIdx * sinTheta > 1.0;
    
    float r0 = (1.0 - refIdx) / (1.0 + refIdx);
    r0 = r0 * r0;
    float r0p = r0 + (1.0 - r0) * pow(1 - cosTheta, 5);
    
    bool refractChance = r0p > fastRandom(Global.seed);
    
    return cannotRefract || refractChance;
}

void refractRay(inout Ray ray, in Payload payload)
{
    float refractionRatio = dot(ray.Direction, payload.HitNormal) < 0.0 ?
        1.0 / Materials[Spheres[payload.HitObject].MaterialId].RefractionRatio :
        Materials[Spheres[payload.HitObject].MaterialId].RefractionRatio;
    
    if (reflectance(ray.Direction, payload.HitNormal, refractionRatio))
    {
        ray.Origin = payload.HitPosition + payload.HitNormal * 0.0001;
        ray.Direction = reflect(ray.Direction, payload.HitNormal);
    }
    else
    {
        ray.Origin = payload.HitPosition - payload.HitNormal * 0.0001;
        ray.Direction = refract(ray.Direction, payload.HitNormal, refractionRatio);
    }
}

void reflectRay(inout Ray ray, in Payload payload)
{
    ray.Origin = payload.HitPosition + payload.HitNormal * 0.0001;
    
    vec3 diffuseDir = normalize(payload.HitNormal + randomUnitSpehere(Global.seed));
    vec3 specularDir = reflect(ray.Direction, payload.HitNormal);
    ray.Direction = mix(diffuseDir, specularDir, Materials[Spheres[payload.HitObject].MaterialId].Roughness);
}

void scatter(inout Ray ray, in Payload payload)
{   
    if (Materials[Spheres[payload.HitObject].MaterialId].RefractionRatio > 1.0)
    {
        refractRay(ray, payload);
    }
    else
    {
        reflectRay(ray, payload);
    }        
}

void main()
{
    vec2 pixelCoord = gl_FragCoord.xy / Resolution;
    vec4 coord = Camera.projection * (2.0 * vec4(pixelCoord, 1.0, 1.0) - 1.0);
    
    Ray precalculatedRay;
    precalculatedRay.Origin = Camera.position;
    precalculatedRay.Direction = vec3(Camera.view * vec4(normalize(coord.xyz / coord.w), 0));
 
    Pixel pixel;
    pixel.Color = vec3(0);
    pixel.Contribution = vec3(1);
    
    for (uint frame = 1; frame <= MaxFrames; frame++)
    {
        Global.seed = uint(gl_FragCoord.y * Resolution.x + gl_FragCoord.x) + frame * FrameIndex * 735529;
        
        Ray ray = precalculatedRay;
        pixel.Contribution = vec3(1);
    
        for (uint i = 0u; i < MaxBounces; i++)
        {
            Global.seed += i;
        
            Payload payload = traceRay(ray);
        
            if (payload.HitObject == -1)
            {
                pixel.Color += payload.Color * pixel.Contribution * DrawEnvironment;
                break;
            }
            
            accumulateColor(pixel, payload.HitObject);
            
            scatter(ray, payload);
        }
    }
    
    pixel.Color = pixel.Color / float(MaxFrames);
    
    if (FrameIndex != 1)
    {
        pixel.Color += texture(AccumulationTexture, TexCoords).rgb;
    }
        
    AccumulationColor = vec4(pixel.Color, 1.0);
    ScreenColor = vec4(AccumulationColor.rgb / float(FrameIndex), 1.0);
}