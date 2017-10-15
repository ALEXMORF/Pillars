/*TODO(Chen):

. Camera rotation limit
 . Player Collision
. standard emitting surface Reflection 
. Anti-aliasing with cone tracing

*/

#include "render.cpp"

struct game_state
{
    GLuint ScreenVAO;
    GLuint ShaderProgram;
    
    v3 SunDir;
    
    v3 PlayerP;
    quaternion PlayerOrientation;
    v3 PlayerLastdP;
    
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
        GameState->ScreenVAO = BuildScreenVAO();
        GameState->ShaderProgram = BuildShaderProgram(VertexShaderSource, FragmentShaderSource);
        GameState->PlayerP = {0.0f, 1.0f, -4.0f};
        GameState->PlayerOrientation = Quaternion();
        GameState->SunDir = Normalize(V3(-0.2f, -1.0f, 0.5f));
        GameState->IsInitialized = true;
    }
    
    GameState->SunDir = Rotate(GameState->SunDir, Quaternion(YAxis(), DegreeToRadian(10.0f * dT)));
    
    //camera orientation
    f32 MouseDeltaAlpha = 0.1f * dT;
    v3 LocalXAxis = Rotate(XAxis(), GameState->PlayerOrientation);
    quaternion XAxisRotation = Quaternion(LocalXAxis, MouseDeltaAlpha * Input->MouseDY);
    quaternion YAxisRotation = Quaternion(YAxis(), MouseDeltaAlpha * Input->MouseDX);
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

