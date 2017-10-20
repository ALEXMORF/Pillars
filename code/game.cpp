/*TODO(Chen):

1. Audio Engine 
2. DOF
3. Motion Blur
4. Fractal men

*/

#include "render.cpp"

const f32 EPSILON = 0.001f;

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

struct world_geometry
{
    shape *Shapes;
    int ShapeCount;
};

internal f32
DE(v3 P, world_geometry *World)
{
    shape *Shapes = World->Shapes;
    int ShapeCount = World->ShapeCount;
    
    f32 DEToShapes = 1000000.0f;
    for (int ShapeIndex = 0; ShapeIndex < ShapeCount; ++ShapeIndex)
    {
        float CurrentDE = Len(P - Shapes[ShapeIndex].P) - Shapes[ShapeIndex].Info.X;
        if (CurrentDE < DEToShapes)
        {
            DEToShapes = CurrentDE;
        }
    }
    
    f32 DEToSphere = DESphere(P);
    f32 DEToBox    = DEBox(P);
    return Min(DEToShapes, Min(DEToSphere, DEToBox));
}

internal v3
DEGradient(v3 P, world_geometry *World)
{
    f32 X = DE({P.X + EPSILON, P.Y, P.Z}, World) - DE({P.X - EPSILON, P.Y, P.Z}, World); 
    f32 Y = DE({P.X, P.Y + EPSILON, P.Z}, World) - DE({P.X, P.Y - EPSILON, P.Z}, World); 
    f32 Z = DE({P.X, P.Y, P.Z + EPSILON}, World) - DE({P.X, P.Y, P.Z - EPSILON}, World); 
    return Normalize(V3(X, Y, Z));
}

struct game_state
{
    renderer Renderer;
    
    v3 SunDir;
    
    v3 PlayerP;
    quaternion PlayerOrientation;
    f32 XRotationInDegrees;
    v3 PlayerLastdP;
    
    quaternion EnemyOrientation;
    v3 EnemyP;
    
    b32 IsInitialized;
};

internal void
UpdateAndRender(void *GameMemory, u32 GameMemorySize, int WindowWidth, int WindowHeight,
                input *Input, f32 dT)
{
    game_state *GameState = (game_state *)GameMemory;
    ASSERT(sizeof(game_state) < GameMemorySize);
    if (!GameState->IsInitialized)
    {
        GameState->Renderer.ScreenVAO = BuildScreenVAO();
        GameState->Renderer.ShaderProgram = BuildShaderProgram(VertexShaderSource, FragmentShaderSource);
        
        GameState->PlayerP = {0.0f, 1.0f, -4.0f};
        GameState->PlayerOrientation = Quaternion();
        GameState->SunDir = Normalize(V3(-0.2f, -1.0f, 0.5f));
        
        GameState->EnemyOrientation = Quaternion();
        GameState->EnemyP = {-2.5f, 0.7f, 0.0f};
        GameState->IsInitialized = true;
    }
    local_persist f32 Time = 0.0f;
    Time += dT;
    
    GameState->SunDir = Rotate(GameState->SunDir, Quaternion(YAxis(), DegreeToRadian(5.0f * dT)));
    
    GameState->EnemyOrientation = Quaternion(YAxis(), DegreeToRadian(10.0f * dT)) * GameState->EnemyOrientation;
    
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
    
    renderer *Renderer = &GameState->Renderer;
    BeginPushShapes(Renderer);
    PushShape(Renderer, {V3(-0.2f, 1.0f, 0.0f), V3(0.5f, 0.0f, 0.0f), V3(1.0f, 0.0f, 0.0f)});
    
    world_geometry World = {};
    World.Shapes = Renderer->Shapes;
    World.ShapeCount = Renderer->ShapeCount;
    
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
    
    f32 CollisionRadius = 0.5f;
    if (DE(GameState->PlayerP, &World) >= CollisionRadius)
    {
        if (DE(GameState->PlayerP + dP, &World) < CollisionRadius)
        {
            v3 SurfaceNormal = DEGradient(GameState->PlayerP, &World);
            dP += Dot(-dP, SurfaceNormal) * SurfaceNormal;
            dP.Y = 0.0f;
            
            //if the corrected displacement still collides, set it to zero
            if (DE(GameState->PlayerP + dP, &World) < CollisionRadius)
            {
                dP = {};
            }
        }
    }
    else
    {
        dP = DEGradient(GameState->PlayerP, &World) * dT;
        dP.Y = 0.0f;
    }
    GameState->PlayerP += dP;
    GameState->PlayerLastdP = dP;
    
    //view matrix upload
    v3 PlayerDir = Rotate(ZAxis(), GameState->PlayerOrientation);
    mat4 View = Mat4LookAt(GameState->PlayerP, GameState->PlayerP + PlayerDir);
    
    RenderWorld(&GameState->Renderer, GameState->PlayerP, GameState->SunDir, View, Time,
                WindowWidth, WindowHeight); 
    glFinish();
}

