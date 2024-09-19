###SHADER COMPUTE
#version 450 core

#extension GL_EXT_nonuniform_qualifier : enable

#define LOWETS_THRESHOLD 1.0e-6F
#define FLT_MAX 3.402823466e+38F
#define UINT_MAX 4294967295.0
#define PI 3.141592653589793
#define FLT_EPS 1.192092896e-07F

layout (local_size_x = 8, local_size_y = 8, local_size_y = 1) in;

layout(set = 0, binding = 0, rgba32f) uniform image2D AccumulationTexture;
layout(set = 0, binding = 1, rgba8) uniform image2D OutTexture;
layout(set = 0, binding = 2) uniform sampler2D SkyMap;

layout(std140, set = 0, binding = 3) uniform Amounts
{
    float DrawEnvironment;
    uint MaxBounces;
    uint MaxFrames;
    uint FrameIndex;
    vec2 Resolution;
    int MaterialsCount;
    int SpheresCount;
    int BVHCount;
    int TrianglesCount;
    int MeshesCount;
    int TexturesCount;
};

layout(std140, set = 0, binding = 4) uniform CameraBuffer
{
    mat4 invProjection;
    mat4 invView;
    vec3 position;
    float focusDistance;
    float defocusStrength;
    float blurStrength;
} Camera;

struct Material
{
    vec3 Albedo;
    vec3 EmmisionColor;
    float Roughness;
    float Metalic;
    float EmmisionPower;
    float RefractionRatio;
    int TextureId;
};

struct Sphere
{
    vec3 Position;
    float Radius;
    int MaterialId;
};

struct Box
{
    vec3 leftBottomFront;
    vec3 rightTopBack;
    uvec2 bufferRegion;
};

struct Triangle
{
    vec3 A;
    vec3 B;
    vec3 C;
    vec2 uvA;
    vec2 uvB;
    vec2 uvC;
};

struct Mesh
{
    uint BVHRoot;
    uint ModelRoot;
    int MaterialId;
};

struct MeshInstance
{
    int MeshId;
};

layout(std140, set = 1, binding = 0) readonly buffer MaterialsBuffer
{
    Material Materials[];
};

layout(std140, set = 1, binding = 1) readonly buffer SpheresBuffer
{
    Sphere Spheres[];
};

layout(std140, set = 1, binding = 2) readonly buffer BoxBuffer
{
    Box Boxes[];
};

layout(std140, set = 1, binding = 3) readonly buffer TriangleBuffer
{
    Triangle Triangles[];
};

layout(std140, set = 1, binding = 4) readonly buffer MeshBuffer
{
    Mesh Meshes[];
};

layout(set = 1, binding = 5) uniform sampler2D Textures[];

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

vec2 randomCirclePoint(inout uint seed)
{
    float angle = fastRandom(seed) * 2 * PI;
    vec2 pointOnCircle = vec2(cos(angle), sin(angle));
    return pointOnCircle * sqrt(fastRandom(seed));
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
    vec3 HitPosition;
    vec3 HitNormal;
    vec2 HitUV;
    float HitDistance;
    int HitObject;
    int HitMaterial;
};

struct HitInfo
{
    float distance;
    bool didHit;
};

struct Pixel
{
    vec3 Color;
    vec3 Contribution;
};

bool isFaceFront(vec3 direction, vec3 surfaceNormal)
{
    return dot(direction, surfaceNormal) < 0.0;
}

vec3 getEmmision(in int matIndex)
{
    return Materials[matIndex].EmmisionColor * Materials[matIndex].EmmisionPower;
}

vec3 getSkyColor(in Ray ray)
{
    //// Basic sky gradient
    // float a = 0.5 * (ray.Direction.y + 1.0);
    // vec3 skyColor = (1.0 - a) * vec3(1.0, 1.0, 1.0) + a * vec3(0.5, 0.7, 1.0);

    //// Sky with sun based on math
    // const vec3 LightDir = normalize(vec3(-1, -1, -1));
    // const vec3 GroundColor = vec3(0.3);
    // const vec3 SkyColorZenith = vec3(0.5, 0.7, 1.0);
    // const vec3 SkyColorHorizon = vec3(0.6, 0.4, 0.4);

    // float skyLerp = pow(smoothstep(0.0, 0.4, ray.Direction.y), 0.35);
    // float groundToSky = smoothstep(-0.01, 0.0, ray.Direction.y);
    // vec3 skyGradient = mix(SkyColorHorizon, SkyColorZenith, skyLerp);
    // float sun = pow(max(0.0, dot(ray.Direction, -LightDir)), 500.0) * 100.0;
    // vec3 skyColor = mix(GroundColor, skyGradient, groundToSky) + sun * float(groundToSky >= 1.0);

    //// SkyMap
    vec2 uv = vec2(atan(ray.Direction.z, ray.Direction.x) / (2.0 * PI), asin(ray.Direction.y) / PI) + 0.5;
    vec3 skyColor = texture(SkyMap, uv).xyz;

    return skyColor;
}

Payload miss(in Ray ray)
{
    Payload payload;
    payload.HitPosition = vec3(0);
    payload.HitNormal = vec3(0);
    payload.HitUV = vec2(0);
    payload.HitDistance = FLT_MAX;
    payload.HitObject = -1;
    payload.HitMaterial = -1;
    
    return payload;
}

Payload closestHit(in Ray ray, in float closestDistance, in int closestObject, in bool isSphere, int closestMesh)
{
    Payload payload;
    payload.HitPosition = ray.Origin + closestDistance * ray.Direction;
    payload.HitDistance = closestDistance;
    payload.HitObject = closestObject;
    
    if (isSphere)
    {
        payload.HitNormal = normalize(payload.HitPosition - Spheres[closestObject].Position);
        payload.HitUV = vec2(atan(payload.HitNormal.z, payload.HitNormal.x) / (2.0 * PI), asin(payload.HitNormal.y) / PI) + 0.5;;
        payload.HitMaterial = Spheres[closestObject].MaterialId;
    }
    else
    {
        vec3 edgeAB = Triangles[closestObject].B - Triangles[closestObject].A;
        vec3 edgeAC = Triangles[closestObject].C - Triangles[closestObject].A;
        vec3 normalVec = cross(edgeAB, edgeAC);
        payload.HitNormal = normalize(normalVec);

        vec3 ao = ray.Origin - Triangles[closestObject].A;
        vec3 dao = cross(ao, ray.Direction);
        float determinant = -dot(ray.Direction, normalVec);
        float invDet = 1 / determinant;
        float u = dot(edgeAC, dao) * invDet;
        float v = -dot(edgeAB, dao) * invDet;
        float w = 1 - u - v;
        
        vec2 texUV =
            Triangles[payload.HitObject].uvA * w +
            Triangles[payload.HitObject].uvB * u +
            Triangles[payload.HitObject].uvC * v;

        payload.HitUV = texUV;

        payload.HitMaterial = Meshes[closestMesh].MaterialId;
        // payload.HitNormal = normalize(vec3(1.0));
    }

    return payload;
}

HitInfo triangleHit(in Ray ray, in Triangle triangle)
{
    vec3 edgeAB = triangle.B - triangle.A;
    vec3 edgeAC = triangle.C - triangle.A;
    vec3 normalVec = cross(edgeAB, edgeAC);
    vec3 ao = ray.Origin - triangle.A;
    vec3 dao = cross(ao, ray.Direction);

    float determinant = -dot(ray.Direction, normalVec);
    float invDet = 1 / determinant;
    
    float t = dot(ao, normalVec) * invDet;
    float u = dot(edgeAC, dao) * invDet;
    float v = -dot(edgeAB, dao) * invDet;
    float w = 1 - u - v;
    
    HitInfo hitInfo;
    hitInfo.didHit = determinant > FLT_EPS && all(greaterThanEqual(vec4(t, u, v, w), vec4(0.0)));
    // hitInfo.normal = normalize(triangle.normalA * w + triangle.normalB * u + triangle.normalC * v);
    hitInfo.distance = t;
    return hitInfo;
}

HitInfo hitBox(in Ray ray, in Box box)
{
    vec3 lb = (box.leftBottomFront - ray.Origin) / ray.Direction;
    vec3 rt = (box.rightTopBack - ray.Origin) / ray.Direction;

    vec3 tMin = min(lb, rt);
    vec3 tMax = max(lb, rt);

    float tNear = max(max(tMin.x, tMin.y), tMin.z);
    float tFar = min(min(tMax.x, tMax.y), tMax.z);

    HitInfo hitInfo;
    hitInfo.didHit = 0 <= tFar && tNear <= tFar;
    hitInfo.distance = tNear;
    return hitInfo;
}

HitInfo bvhTraverse(in Ray ray, in uint bvhRoot, in uint modelRoot, out int closestObject)
{
    HitInfo meshHit = hitBox(ray, Boxes[bvhRoot]);
    if (!meshHit.didHit)
    {
        return meshHit;
    }

    HitInfo returnInfo;
    returnInfo.distance = FLT_MAX;
    returnInfo.didHit = false;

    const uint maxDepth = 32u;
    uint stack[maxDepth];
    uint stackIdx = 0u;

    stack[stackIdx] = bvhRoot;
    stackIdx++;

    while (stackIdx > 0u)
    {
        stackIdx--;
        Box box = Boxes[stack[stackIdx]];

        bool isLeaf = box.bufferRegion.y > 0u;

        if (isLeaf)
        {
            for (uint triangleId = box.bufferRegion.x; triangleId <= box.bufferRegion.y; triangleId++)
            {
                HitInfo hitInfo = triangleHit(ray, Triangles[modelRoot + triangleId]);
                if (hitInfo.didHit && hitInfo.distance < returnInfo.distance)
                {
                    returnInfo.distance = hitInfo.distance;
                    closestObject = int(triangleId);
                    returnInfo.didHit = true;
                }
            }
        }
        else
        {

        }
    }

    // returnInfo.distance = 1.0;
    // returnInfo.didHit = true;
    return returnInfo;
}

HitInfo sphereHit(in Ray ray, in Sphere sphere)
{
    HitInfo hitInfo;
    
    vec3 origin = ray.Origin - sphere.Position;

    float a = dot(ray.Direction, ray.Direction);
    float b = 2.0 * dot(origin, ray.Direction);
    float c = dot(origin, origin) - pow(sphere.Radius, 2.0);
    float delta = b * b - 4.0 * a * c;

    if (delta < 0.0)
    {
        hitInfo.didHit = false;
        return hitInfo;
    }

    float closestT = (-b - sqrt(delta)) / (2.0 * a);

    if (closestT < 0.0)
    {
        hitInfo.didHit = false;
        return hitInfo;
    }

    hitInfo.didHit = true;
    hitInfo.distance = closestT;
    return hitInfo;
}

Payload bounceRay(in Ray ray)
{
    float closestDistance = FLT_MAX;
    int closestMesh = -1;
    int closestObject = -1;
    bool isSphere = false;
    
    for (int sphereId = 0; sphereId < SpheresCount; sphereId++)
    {
        HitInfo hitInfo = sphereHit(ray, Spheres[sphereId]);
        if (hitInfo.didHit && hitInfo.distance < closestDistance)
        {
            closestDistance = hitInfo.distance;
            closestObject = sphereId;
            isSphere = true;
        }
    }
    
    for (int meshId = 0; meshId < MeshesCount; meshId++)
    {
        int cloObject = -1;
        HitInfo meshHit = bvhTraverse(ray, Meshes[meshId].BVHRoot, Meshes[meshId].ModelRoot, cloObject);

        if (meshHit.didHit && meshHit.distance < closestDistance)
        {
            closestDistance = meshHit.distance;
            closestMesh = meshId;
            closestObject = cloObject;
            isSphere = false;
        }
    }
    
    if (closestObject == -1)
        return miss(ray);
    
    return closestHit(ray, closestDistance, closestObject, isSphere, closestMesh);
}

void accumulateColor(inout Pixel pixel, in Payload payload)
{
    vec3 albedo = vec3(0);
    int texId = Materials[payload.HitMaterial].TextureId;
    if (-1 != texId)
    {
        albedo = texture(Textures[texId], payload.HitUV).xyz;
        pixel.Color += albedo * Materials[payload.HitMaterial].EmmisionPower * pixel.Contribution;
    }
    else
    {
        pixel.Color += getEmmision(payload.HitMaterial) * pixel.Contribution;
        albedo = Materials[payload.HitMaterial].Albedo;
    }
    pixel.Contribution *= albedo;
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
    float refractionRatio = Materials[payload.HitMaterial].RefractionRatio;
    bool isFront = isFaceFront(ray.Direction, payload.HitNormal);
    
    float rt = isFront ? 1.0 / refractionRatio : refractionRatio;
    vec3 hitNormal = isFront ? payload.HitNormal : -payload.HitNormal;

    if (reflectance(ray.Direction, hitNormal, rt))
    {
        ray.Origin = payload.HitPosition + hitNormal * 0.0001;
        ray.Direction = reflect(ray.Direction, hitNormal);
    }
    else
    {
        ray.Origin = payload.HitPosition - hitNormal * 0.0001;
        ray.Direction = refract(ray.Direction, hitNormal, rt);
    }
}

void reflectRay(inout Ray ray, in Payload payload)
{
    ray.Origin = payload.HitPosition + payload.HitNormal * 0.0001;
    
    vec3 diffuseDir = normalize(payload.HitNormal + randomUnitSpehere(Global.seed));
    vec3 specularDir = normalize(reflect(ray.Direction, payload.HitNormal) + randomUnitSpehere(Global.seed) * (1.0 - Materials[payload.HitMaterial].Metalic));
    
    ray.Direction = mix(diffuseDir, specularDir, Materials[payload.HitMaterial].Roughness);
    ray.Direction = normalize(ray.Direction);
}

void scatter(inout Ray ray, in Payload payload, inout Pixel pixel)
{
    if (Materials[payload.HitMaterial].RefractionRatio > 1.0)
    {
        refractRay(ray, payload);
    }
    else
    {
        reflectRay(ray, payload);
    }

    accumulateColor(pixel, payload);
}

vec3 traceRay(in Ray ray)
{
    Pixel pixel;
    pixel.Color = vec3(0);
    pixel.Contribution = vec3(1);

    for (uint i = 0u; i < MaxBounces; i++)
    {
        Global.seed += i;
    
        Payload payload = bounceRay(ray);
    
        if (payload.HitObject == -1)
        {
            pixel.Color += getSkyColor(ray) * pixel.Contribution * DrawEnvironment;
            break;
        }
        
        scatter(ray, payload, pixel);
    }

    return pixel.Color;
}

void main()
{
    ivec2 index = ivec2(gl_GlobalInvocationID.xy);
    ivec2 screenSize = imageSize(AccumulationTexture);
    if (any(greaterThan(index, screenSize)))
    {
        return;
    }

    const vec3 rightVec = Camera.invView[0].xyz;
    const vec3 upVec = Camera.invView[1].xyz;

    vec2 pixelCoord = vec2(index) / Resolution;
    vec4 coord = Camera.invProjection * (2.0 * vec4(pixelCoord, 1.0, 1.0) - 1.0);

    vec3 direction = vec3(Camera.invView * vec4(coord.xyz / coord.w, 0)) * Camera.focusDistance;
    vec3 focusPoint = Camera.position + direction;

    vec3 incomingLight = vec3(0.0);

    for (uint frame = 1; frame <= MaxFrames; frame++)
    {
        Global.seed = uint(index.y * Resolution.x + index.x) + frame * FrameIndex * 735529;
        
        vec2 focusJitter = randomCirclePoint(Global.seed) / Resolution * Camera.defocusStrength;
        vec2 deviationJitter = randomCirclePoint(Global.seed) / Resolution * Camera.blurStrength;

        vec3 deviationJitterFocusPoint = focusPoint + deviationJitter.x * rightVec + deviationJitter.y * upVec;

        Ray cameraRay;
        cameraRay.Origin = Camera.position + focusJitter.x * rightVec + focusJitter.y * upVec;
        cameraRay.Direction = normalize(deviationJitterFocusPoint - cameraRay.Origin);

        incomingLight += traceRay(cameraRay);
    }
    
    incomingLight = incomingLight / float(MaxFrames);
    if (FrameIndex != 1)
    {
        incomingLight += imageLoad(AccumulationTexture, index).rgb;
    }
    
    vec3 outColor = incomingLight / float(FrameIndex);
    // outColor = sqrt(outColor);
    
    imageStore(AccumulationTexture, index, vec4(incomingLight, 1.0));
    imageStore(OutTexture, index, vec4(outColor, 1.0));
}
