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
const int MAX_MARCH_STEP = 200;
const float MAX_DEPTH = 30.0;

uniform vec3 CameraP;
uniform vec3 LightDir;

in vec3 FragViewRay;
out vec3 OutColor;

float DESphere(vec3 P)
{
P -= 1.0;
return length(P) - 1.0;
}

float DEPlane(vec3 P)
{
const vec3 Normal = vec3(0.0, 1.0, 0.0);
return dot(Normal, P);
}

float DEBox(vec3 P)
{
#if 1
P.y -= 0.5;

P.xz = mod(P.xz, 5.0) - 2.5;
#else
P.y -= 3.5;

P.xyz = mod(P.xyz, 6.0) - 3.0;
#endif

const vec3 B = vec3(1.0, 1.0, 1.0);
const float R = 0.1;
return length(max(abs(P)-B, 0.0)) - R;
}

float DE(vec3 P)
{
return min(DEPlane(P), min(DESphere(P), DEBox(P)));
}

vec3 DEGradient(vec3 P)
{
return normalize(vec3(
DE(vec3(P.x + EPSILON, P.y, P.z)) - DE(vec3(P.x - EPSILON, P.y, P.z)),
DE(vec3(P.x, P.y + EPSILON, P.z)) - DE(vec3(P.x, P.y - EPSILON, P.z)),
DE(vec3(P.x, P.y, P.z + EPSILON)) - DE(vec3(P.x, P.y, P.z - EPSILON))
));
}

float CalcVisiblity(vec3 P, vec3 LightDir, float LightDist, float K)
{
float Visiblity = 1.0;

vec3 StartP = P - LightDir * LightDist;
float DepthBias = 100.0*EPSILON;
for (float LightDepth = 0.0; LightDepth < LightDist-DepthBias;)
{
float Dist = DE(StartP + LightDir * LightDepth);
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
float ClosestDist = DE(P + Normal * NormalDist);
AmbientVisibility -= (1 / pow(2, float(I))) * (NormalDist - ClosestDist);
}

return AmbientVisibility;
}

void main()
{
vec3 ViewRay = normalize(FragViewRay);
const vec3 SkyColor = vec3(0.2);

bool RayHit = false;
float Depth = 0.0;
for (int I = 0; I < MAX_MARCH_STEP && Depth < MAX_DEPTH; ++I)
{
float Dist = DE(CameraP + Depth * ViewRay);
if (Dist < EPSILON)
{
RayHit = true;
break;
}
Depth += Dist;
}

if (RayHit)
{
vec3 HitP = CameraP + Depth * ViewRay;

float LightDist = 10.0;
float ShadowK = 8.0;
float Visiblity = CalcVisiblity(HitP, LightDir, LightDist, ShadowK);

vec3 Normal = DEGradient(HitP);
float AmbientVisiblity = CalcAmbientVisibility(HitP, Normal);

vec3 Color = vec3(0.8, 0.8, 0.8);
vec3 Ambient = 0.3 * Color;
vec3 Diffuse = 0.7 * Color * max(dot(Normal, -LightDir), 0.0);
OutColor = AmbientVisiblity * Ambient + Visiblity * Diffuse;
OutColor = mix(OutColor, SkyColor, min(Depth / MAX_DEPTH, 1.0));
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


