/*TODO(Chen):

. Mouse jitter when window is resized
. Camera oriented movement instead of oriented along Z Axis
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
    v3 CameraP;
    v3 CameraLookDir;
    v3 LightDir;
    v3 LastdP;
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
        GameState->CameraP = {0.0f, 1.0f, -4.0f};
        GameState->CameraLookDir = ZAxis();
        GameState->LightDir = Normalize(V3(-0.2f, -1.0f, 0.5f));
        GameState->IsInitialized = true;
    }
    
    //GameState->LightDir = Rotate(GameState->LightDir, Quaternion(YAxis(), DegreeToRadian(1.0f * dT)));
    
    v3 dP = {};
    if (Input->Left) dP.X -= 1.0f;
    if (Input->Right) dP.X += 1.0f;
    if (Input->Up) dP.Z += 1.0f;
    if (Input->Down) dP.Z -= 1.0f;
    dP = Lerp(GameState->LastdP, Normalize(dP) * 2.0f * dT, 0.15f);
    GameState->CameraP += dP;
    GameState->LastdP = dP;
    
    f32 MouseDeltaAlpha = 0.1f * dT;
    GameState->CameraLookDir = Rotate(GameState->CameraLookDir, 
                                      Quaternion(XAxis(), MouseDeltaAlpha * Input->MouseDY));
    GameState->CameraLookDir = Rotate(GameState->CameraLookDir, 
                                      Quaternion(YAxis(), MouseDeltaAlpha * Input->MouseDX));
    mat4 View = Mat4LookAt(GameState->CameraP, GameState->CameraP + GameState->CameraLookDir);
    
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    
    glUseProgram(GameState->ShaderProgram);
    glUploadMatrix4(GameState->ShaderProgram, "ViewRotation", &View);
    glUploadVec3(GameState->ShaderProgram, "CameraP", GameState->CameraP);
    glUploadVec3(GameState->ShaderProgram, "LightDir", GameState->LightDir);
    glUploadVec2(GameState->ShaderProgram, "ScreenSize", V2(WindowWidth, WindowHeight));
    glBindVertexArray(GameState->ScreenVAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
    
    glFinish();
}

