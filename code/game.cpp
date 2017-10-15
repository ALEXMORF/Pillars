/*TODO(Chen):

 . reflection
. standard emitting surface Reflection 
. Anti-aliasing with cone tracing

*/

#include "render.cpp"

const f32 EPSILON = 0.001f;

struct game_state
{
    GLuint ScreenVAO;
    GLuint ShaderProgram;
    
    v3 SunDir;
    
    v3 PlayerP;
    quaternion PlayerOrientation;
    f32 XRotationInDegrees;
    v3 PlayerLastdP;
    
    b32 IsInitialized;
};

internal f32
DESphere(v3 P)
{
    P -= {0.95f, 0.95f, 0.95f};
    return Len(P) - 1.0f;
}

internal f32
DEBox(v3 P)
{
    P.Y -= 0.5f;
    P.X = P.X - 5.5f * floorf(P.X / 5.5f) - 2.75f;
    P.Z = P.Z - 5.5f * floorf(P.Z / 5.5f) - 2.75f;
    
    const v3 B = {1.0f, 1.0f, 1.0f};
    const f32 R = 0.1f;
    
    v3 TestV = {};
    TestV.X = Max(fabsf(P.X) - B.X, 0.0f);
    TestV.Y = Max(fabsf(P.Y) - B.Y, 0.0f);
    TestV.Z = Max(fabsf(P.Z) - B.Z, 0.0f);
    
    return Len(TestV) - R;
}

internal f32
DE(v3 P)
{
    f32 DEToSphere = DESphere(P);
    f32 DEToBox    = DEBox(P);
    return DEToSphere < DEToBox? DEToSphere: DEToBox;
}

internal v3
DEGradient(v3 P)
{
    f32 X = DE({P.X + EPSILON, P.Y, P.Z}) - DE({P.X - EPSILON, P.Y, P.Z}); 
    f32 Y = DE({P.X, P.Y + EPSILON, P.Z}) - DE({P.X, P.Y - EPSILON, P.Z}); 
    f32 Z = DE({P.X, P.Y, P.Z + EPSILON}) - DE({P.X, P.Y, P.Z - EPSILON}); 
    return Normalize(V3(X, Y, Z));
}

internal void
UpdateAndRender(void *GameMemory, u32 GameMemorySize, int WindowWidth, int WindowHeight,
                input *Input, f32 dT)
{
    game_state *GameState = (game_state *)GameMemory;
    ASSERT(sizeof(game_state) < GameMemorySize);
    if (!GameState->IsInitialized)
    {
        GameState->ScreenVAO = BuildScreenVAO();
        GameState->ShaderProgram = BuildShaderProgram(VertexShaderSource, FragmentShaderSource);
        GameState->PlayerP = {0.0f, 1.0f, -4.0f};
        GameState->PlayerOrientation = Quaternion();
        GameState->SunDir = Normalize(V3(-0.2f, -1.0f, 0.5f));
        GameState->IsInitialized = true;
    }
    
    GameState->SunDir = Rotate(GameState->SunDir, Quaternion(YAxis(), DegreeToRadian(10.0f * dT)));
    
    //camera orientation
    f32 MouseDeltaAlpha = 2.0f * dT;
    f32 XRotationInDegrees = MouseDeltaAlpha * Input->MouseDY;
    f32 YRotationInDegrees = MouseDeltaAlpha * Input->MouseDX;
    if (fabs(GameState->XRotationInDegrees + XRotationInDegrees) > 45.0f)
    {
        XRotationInDegrees = 0.0f;
    }
    else
    {
        GameState->XRotationInDegrees += XRotationInDegrees;
    }
    v3 LocalXAxis = Rotate(XAxis(), GameState->PlayerOrientation);
    quaternion XAxisRotation = Quaternion(LocalXAxis, DegreeToRadian(XRotationInDegrees));
    quaternion YAxisRotation = Quaternion(YAxis(), DegreeToRadian(YRotationInDegrees));
    GameState->PlayerOrientation = YAxisRotation * XAxisRotation * GameState->PlayerOrientation;
    
    //movement 
    v3 Forward = Rotate(ZAxis(), GameState->PlayerOrientation);
    {
        Forward.Y = 0.0f;
        Forward = Normalize(Forward);
    }
    v3 Right = Cross(YAxis(), Forward);
    v3 dP = {};
    if (Input->Left) dP += -Right;
    if (Input->Right) dP += Right;
    if (Input->Up) dP += Forward;
    if (Input->Down) dP += -Forward;
    dP = Lerp(GameState->PlayerLastdP, Normalize(dP) * 2.0f * dT, 0.15f);
    if (DE(GameState->PlayerP + dP) < 0.5f)
    {
        v3 SurfaceNormal = DEGradient(GameState->PlayerP);
        dP += Dot(-dP, SurfaceNormal) * SurfaceNormal;
        dP.Y = 0.0f;
    }
    GameState->PlayerP += dP;
    GameState->PlayerLastdP = dP;
    
    //view matrix upload
    v3 PlayerDir = Rotate(ZAxis(), GameState->PlayerOrientation);
    mat4 View = Mat4LookAt(GameState->PlayerP, GameState->PlayerP + PlayerDir);
    
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    
    glUseProgram(GameState->ShaderProgram);
    glUploadMatrix4(GameState->ShaderProgram, "ViewRotation", &View);
    glUploadVec3(GameState->ShaderProgram, "PlayerP", GameState->PlayerP);
    glUploadVec3(GameState->ShaderProgram, "SunDir", GameState->SunDir);
    glUploadVec2(GameState->ShaderProgram, "ScreenSize", V2(WindowWidth, WindowHeight));
    glBindVertexArray(GameState->ScreenVAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
    
    glFinish();
}

