char *VertexShaderSource = R"(
#version 400 core

uniform vec2 ScreenSize;
uniform mat4 ViewRotation;

layout (location = 0) in vec3 P;
layout (location = 1) in vec2 TexCoord;

out vec3 FragViewRay;

void main()
{
const float FOV = 45.0;
const float HFOV = FOV * 0.5;

float AspectRatio = ScreenSize.x / ScreenSize.y;
FragViewRay.x = P.x * AspectRatio;
FragViewRay.y = P.y;
FragViewRay.z = 1.0 / tan(radians(HFOV));

FragViewRay *= inverse(mat3(ViewRotation));

gl_Position = vec4(P, 1.0);
}

)";

char *FragmentShaderSource = R"(
#version 400 core

const float EPSILON = 0.001;
const int MAX_MARCH_STEP = 500;
const float MAX_DEPTH = 40.0;
const float MIN_DEPTH = 0.1;
const float PI = 3.1451926;

struct shape
{
vec3 P;
vec3 Info;
vec3 Material;
};

uniform vec3 PlayerP;
uniform vec3 SunDir;
uniform int ShapeCount;
uniform shape Shapes[50];
uniform float Time;

in vec3 FragViewRay;
out vec3 OutColor;

mat4 rotationMatrix(vec3 axis, float angle)
{
axis = normalize(axis);
float s = sin(angle);
float c = cos(angle);
float oc = 1.0 - c;

return mat4(oc * axis.x * axis.x + c,           oc * axis.x * axis.y - axis.z * s,  oc * axis.z * axis.x + axis.y * s,  0.0,
oc * axis.x * axis.y + axis.z * s,  oc * axis.y * axis.y + c,           oc * axis.y * axis.z - axis.x * s,  0.0,
oc * axis.z * axis.x - axis.y * s,  oc * axis.y * axis.z + axis.x * s,  oc * axis.z * axis.z + c,           0.0,
0.0,                                0.0,                                0.0,                                1.0);
}

float sdCapsule( vec3 p, vec3 a, vec3 b, float r )
{
vec3 pa = p - a, ba = b - a;
float h = clamp( dot(pa,ba)/dot(ba,ba), 0.0, 1.0 );
return length( pa - ba*h ) - r;
}

float DEPrism(vec3 P, vec2 H)
{
P.y -= 2.2;
P = mat3(inverse(rotationMatrix(vec3(1.0, 0.0, 0.0), 0.5 * PI))) * P;
vec3 q = abs(P);
return max(q.z-H.y,max(q.x*0.866025+P.y*0.5,-P.y)-H.x*0.5);
}

float DEPerson(vec3 P)
{
float DistToHead = length(P - vec3(0, 2, 0)) - 0.3;

float BodyScale = 0.5;
float LegScale  = 0.4;
float DistToBody = DEPrism(P/BodyScale, vec2(1, 1))*BodyScale;
float DistToLeftArm = sdCapsule(P - vec3(-0.3, -0.8, -1.0), vec3(1.0, 2.2, 1.0), vec3(1.0), 0.2);
float DistToRightArm = sdCapsule(P - vec3(-1.7, -0.8, -1.0), vec3(1.0, 2.2, 1.0), vec3(1.0), 0.2);
float DistToLeg = DEPrism(P/LegScale - vec3(0.0, -2.5, 0.0), vec2(1, 1))*BodyScale;

return min(DistToLeg, min(DistToRightArm, min(DistToLeftArm, min(DistToBody, DistToHead))));
}

float DESphere(vec3 P)
{
P -= 0.95;
return length(P) - 1.0;
}

float DEPlane(vec3 P)
{
const vec3 Normal = vec3(0.0, 1.0, 0.0);
return dot(Normal, P);
}

float DEBox(vec3 P)
{
P.y -= 0.5;
P.xz = mod(P.xz, 5.5) - 2.75;

const vec3 B = vec3(1.0, 15.0, 1.0);
const float R = 0.1;
return length(max(abs(P)-B, 0.0)) - R;
}

struct DE_info
{
float Dist;
vec3 Material;
};

DE_info DE(vec3 P)
{
DE_info Result;

vec3 Material;
float DEToShapes = 10000000.0f;
for (int ShapeIndex = 0; ShapeIndex < ShapeCount; ++ShapeIndex)
{
float CurrentDE = length(P - Shapes[ShapeIndex].P) - Shapes[ShapeIndex].Info.x;
if (CurrentDE < DEToShapes)
{
DEToShapes = CurrentDE;
Material = Shapes[ShapeIndex].Material;
}
}

float DEToScene = min(DEPlane(P), min(DESphere(P), DEBox(P)));
if (DEToScene < DEToShapes)
{
Result.Dist = DEToScene;
Result.Material = vec3(0.8, 0.8, 0.8);
}
else
{
Result.Dist = DEToShapes;
Result.Material = Material;
}
return Result;
}

vec3 DEGradient(vec3 P)
{
return normalize(vec3(
DE(vec3(P.x + EPSILON, P.y, P.z)).Dist - DE(vec3(P.x - EPSILON, P.y, P.z)).Dist,
DE(vec3(P.x, P.y + EPSILON, P.z)).Dist - DE(vec3(P.x, P.y - EPSILON, P.z)).Dist,
DE(vec3(P.x, P.y, P.z + EPSILON)).Dist - DE(vec3(P.x, P.y, P.z - EPSILON)).Dist
));
}

float CalcVisiblity(vec3 P, vec3 LightDir, float LightDist, float K)
{
float Visiblity = 1.0;

vec3 StartP = P - LightDir * LightDist;
float DepthBias = 100.0*EPSILON;
for (float LightDepth = MIN_DEPTH; LightDepth < LightDist-DepthBias;)
{
float Dist = DE(StartP + LightDir * LightDepth).Dist;
if (Dist < EPSILON)
{
Visiblity = 0.0;
break;
}
Visiblity = min(Visiblity, K * Dist / (LightDist - LightDepth));
LightDepth += Dist;
}

return Visiblity;
}

float CalcAmbientVisibility(vec3 P, vec3 Normal)
{
float AmbientVisibility = 1.0;

float Delta = 0.15;

for (int I = 1; I <= 5; ++I)
{
float NormalDist = Delta * float(I);
float ClosestDist = DE(P + Normal * NormalDist).Dist;
AmbientVisibility -= (1 / pow(2, float(I))) * (NormalDist - ClosestDist);
}

return AmbientVisibility;
}

struct ray_info
{
vec3 HitP;
float Depth;
bool DidHit;
vec3 Material;
};

ray_info ShootRay(vec3 StartP, vec3 Dir)
{
ray_info Result;

float Depth = MIN_DEPTH;
for (int I = 0; I < MAX_MARCH_STEP && Depth < MAX_DEPTH; ++I)
{
DE_info DEInfo = DE(StartP + Depth * Dir);
float Dist = DEInfo.Dist;
if (Dist < EPSILON)
{
Result.DidHit = true;
Result.Material = DEInfo.Material;
break;
}
Depth += Dist;
}

Result.HitP = StartP + Depth * Dir;
Result.Depth = Depth;
return Result;
}

vec3 SunShading(ray_info Ray, vec3 Normal, vec3 LightDir)
{
float LightDist = 1000.0;
float ShadowK = 8.0;
float Visiblity = CalcVisiblity(Ray.HitP, LightDir, LightDist, ShadowK);

float AmbientVisiblity = CalcAmbientVisibility(Ray.HitP, Normal);

vec3 Color = Ray.Material;
vec3 Ambient = 0.3 * Color;
vec3 Diffuse = 0.7 * Color * max(dot(Normal, -LightDir), 0.0);

return AmbientVisiblity * (Ambient + Visiblity * Diffuse);
}

vec3 LightShading(ray_info Ray, vec3 Normal, vec3 LightP, vec3 LightColor)
{
float LightDist = distance(LightP, Ray.HitP);
vec3 LightDir = normalize(Ray.HitP - LightP);

#if 1
float Visiblity = 1.0;
#else
float ShadowK = 8.0;
float Visiblity = CalcVisiblity(Ray.HitP, LightDir, LightDist, ShadowK);
#endif

float AmbientVisiblity = CalcAmbientVisibility(Ray.HitP, Normal);

vec3 Color = Ray.Material * LightColor;
float LightK = 1.0;
vec3 Diffuse = Color * max(dot(Normal, -LightDir), 0.0) * (1 / pow(LightK * LightDist, 2));

return Visiblity * Diffuse;
}

void main()
{

vec3 ViewRay = normalize(FragViewRay);
const vec3 SkyColor = vec3(0.0);

ray_info Ray = ShootRay(PlayerP, ViewRay);
if (Ray.DidHit)
{
vec3 Normal = DEGradient(Ray.HitP);
vec3 LightBounce = SunShading(Ray, Normal, SunDir);
OutColor = mix(LightBounce, SkyColor, min(Ray.Depth / MAX_DEPTH, 1.0));
}
else
{
OutColor = SkyColor;
}
}

)";

internal GLuint
ReadAndCompileShader(char *Source, GLenum Type)
{
    GLuint Shader = glCreateShader(Type);
    
    glShaderSource(Shader, 1, &Source, 0);
    glCompileShader(Shader);
    GLint CompileStatus = 0;
    glGetShaderiv(Shader, GL_COMPILE_STATUS, &CompileStatus);
    if (CompileStatus != GL_TRUE)
    {
        GLsizei ErrorMsgLength = 0;
        GLchar ErrorMsg[1024];
        glGetShaderInfoLog(Shader, sizeof(ErrorMsg), 
                           &ErrorMsgLength, ErrorMsg);
        ASSERT(!"fail to compile current shader");
    }
    
    return Shader;
}

internal GLuint
BuildShaderProgram(char *VertShaderSource, char *FragShaderSource)
{
    GLuint VertShader = ReadAndCompileShader(VertShaderSource, GL_VERTEX_SHADER);
    GLuint FragShader = ReadAndCompileShader(FragShaderSource, GL_FRAGMENT_SHADER);
    //TODO(Chen): link up to a program then delete & detach the shaders
    
    GLuint Program = glCreateProgram();
    glAttachShader(Program, VertShader);
    glAttachShader(Program, FragShader);
    glLinkProgram(Program);
    GLint ProgramLinked = 0;
    glGetProgramiv(Program, GL_LINK_STATUS, &ProgramLinked);
    if (ProgramLinked != GL_TRUE)
    {
        GLsizei ErrorMsgLength = 0;
        GLchar ErrorMsg[1024];
        glGetProgramInfoLog(Program, sizeof(ErrorMsg),
                            &ErrorMsgLength, ErrorMsg);
        ASSERT(!"Failed to link shaders");
    }
    
    glDetachShader(Program, VertShader);
    glDeleteShader(VertShader);
    glDetachShader(Program, FragShader);
    glDeleteShader(FragShader);
    
    return Program;
}

struct shape
{
    v3 P;
    v3 Info;
    v3 Material;
};

struct renderer
{
    GLuint ScreenVAO;
    GLuint ShaderProgram;
    
    shape Shapes[50];
    int ShapeCount;
};

internal void
BeginPushShapes(renderer *Renderer)
{
    Renderer->ShapeCount = 0;
}

internal void
PushShape(renderer *Renderer, shape Shape)
{
    ASSERT(Renderer->ShapeCount < ARRAY_COUNT(Renderer->Shapes));
    Renderer->Shapes[Renderer->ShapeCount++] = Shape;
}

internal void
RenderWorld(renderer *Renderer, v3 PlayerP, v3 SunDir, mat4 View, f32 Time, 
            int WindowWidth, int WindowHeight)
{
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glUseProgram(Renderer->ShaderProgram);
    
    //upload shapes
    glUploadInt32(Renderer->ShaderProgram, "ShapeCount", Renderer->ShapeCount);
    char IdentifierID[100];
    for (int ShapeIndex = 0; ShapeIndex < Renderer->ShapeCount; ++ShapeIndex)
    {
        shape *Shape = Renderer->Shapes + ShapeIndex;
        
        snprintf(IdentifierID, sizeof(IdentifierID), "Shapes[%d].P", ShapeIndex);
        glUploadVec3(Renderer->ShaderProgram, IdentifierID, Shape->P);
        
        snprintf(IdentifierID, sizeof(IdentifierID), "Shapes[%d].Info", ShapeIndex);
        glUploadVec3(Renderer->ShaderProgram, IdentifierID, Shape->Info);
        
        snprintf(IdentifierID, sizeof(IdentifierID), "Shapes[%d].Material", ShapeIndex);
        glUploadVec3(Renderer->ShaderProgram, IdentifierID, Shape->Material);
    }
    
    //uploadt renderer settings
    glUploadMatrix4(Renderer->ShaderProgram, "ViewRotation", &View);
    glUploadVec3(Renderer->ShaderProgram, "PlayerP", PlayerP);
    glUploadVec3(Renderer->ShaderProgram, "SunDir", SunDir);
    glUploadVec2(Renderer->ShaderProgram, "ScreenSize", V2(WindowWidth, WindowHeight));
    glBindVertexArray(Renderer->ScreenVAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
    }