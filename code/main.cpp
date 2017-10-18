#include "kernel.h"
#include "platform.h"
#include "game.cpp"

#include <windows.h>
#include <windowsx.h>
#include "win32_kernel.h"

global_variable b32 gGameIsRunning;
global_variable int gWindowWidth;
global_variable int gWindowHeight;
global_variable b32 gFirstMouseMovement;

LRESULT CALLBACK
Win32WindowCallback(HWND Window, UINT Message, WPARAM WParam, LPARAM LParam)
{
    LRESULT Result = 0;
    switch (Message)
    {
        case WM_CLOSE: 
        {
            gGameIsRunning = false;
        } break;
        
        case WM_SIZE:
        {
            gWindowWidth = LOWORD(LParam);
            gWindowHeight = HIWORD(LParam);
            if (glViewport)
            {
                glViewport(0, 0, gWindowWidth, gWindowHeight);
            }
            gFirstMouseMovement = true;
        } break;
        
        default:
        {
            Result = DefWindowProc(Window, Message, WParam, LParam);
        } break;
    }
    return Result;
}

int CALLBACK
WinMain(HINSTANCE CurrentInstance, HINSTANCE, LPSTR CommandLine, int ShowCode)
{
    gWindowWidth = 800;
    gWindowHeight = 600;
    f32 FPS = 60.0f;
    f32 DesiredFrameTimeInMS = 1000.0f / 60.0f;
    
    HWND Window = Win32CreateWindow(0, gWindowWidth, gWindowHeight, "Game Jam", "Game Jam window", Win32WindowCallback);
    HDC WindowDC = GetDC(Window);
    
    u32 GameMemorySize = MB(64);
    void *GameMemory = Win32AllocateMemory(GameMemorySize);
    
    Win32InitializeOpengl(WindowDC, 4, 0);
    LoadGLFunctions(Win32GetOpenglFunction);
    wgl_swap_interval_ext *SwapInterval = (wgl_swap_interval_ext *)Win32GetOpenglFunction("wglSwapIntervalEXT");
    SwapInterval(1);
    glViewport(0, 0, gWindowWidth, gWindowHeight);
    
    input Input = {};
    
    gFirstMouseMovement = true;
    int LastMouseX = 0;
    int LastMouseY = 0;
    f32 LastFrameTimeInMS = 0.0f;
    gGameIsRunning = true;
    while (gGameIsRunning)
    {
        u64 BeginCounter = Win32GetPerformanceCounter();
        
        b32 MouseMoved = false;
        MSG Message;
        while (PeekMessage(&Message, 0, 0, 0, PM_REMOVE))
        {
            switch (Message.message)
            {
                case WM_MOUSEMOVE:
                {
                    RECT ClientRect = Win32GetClientRect(Window);
                    
                    int NewMouseX = GET_X_LPARAM(Message.lParam);
                    int NewMouseY = GET_Y_LPARAM(Message.lParam);
                    int MouseDX = NewMouseX - LastMouseX;
                    int MouseDY = NewMouseY - LastMouseY;
                    LastMouseX = NewMouseX;
                    LastMouseY = NewMouseY;
                    
                    MouseMoved = true;
                    if (!gFirstMouseMovement)
                    {
                        Input.MouseDX = MouseDX;
                        Input.MouseDY = MouseDY;
                    }else
                    {
                        gFirstMouseMovement = false;
                    }
                } break;
                
                case WM_KEYUP:
                case WM_KEYDOWN:
                case WM_SYSKEYDOWN:
                case WM_SYSKEYUP:
                {
                    b32 KeyDown = (Message.lParam & (1 << 31)) == 0;
                    b32 KeyWasDown = (Message.lParam & ( 1 << 30)) != 0;
                    b32 AltKeyDown = (Message.lParam & (1 << 29)) == 1;
                    
                    WPARAM KeyCode = Message.wParam;
                    
                    if (KeyDown && !KeyWasDown) 
                    {
                        if (KeyCode == VK_ESCAPE)
                        {
                            gGameIsRunning = false;
                        }
                        if (KeyCode == VK_F11)
                        {
                            Win32ToggleFullscreen(Window);
                        }
                    }
                    
                    if (KeyDown != KeyWasDown)
                    {
                        if (KeyCode == 'A')
                        {
                            Input.Left = KeyDown;
                        }
                        if (KeyCode == 'D')
                        {
                            Input.Right = KeyDown;
                        }
                        if (KeyCode == 'W')
                        {
                            Input.Up = KeyDown;
                        }
                        if (KeyCode == 'S')
                        {
                            Input.Down = KeyDown;
                        }
                    }
                } break;
                
                default:
                {
                    TranslateMessage(&Message);
                    DispatchMessage(&Message);
                } break;
            }
        }
        
        if (!MouseMoved)
        {
            Input.MouseDX = 0;
            Input.MouseDY = 0;
        }
        
        //Locking the mouse
        {
            ShowCursor(FALSE);
            
            RECT ClientRect = Win32GetClientRect(Window);
            
            int LockMouseX = (ClientRect.right + ClientRect.left) / 2;
            int LockMouseY = (ClientRect.bottom + ClientRect.top) / 2;
            
            SetCursorPos(LockMouseX, LockMouseY);
            LastMouseX = LockMouseX - ClientRect.left;
            LastMouseY = LockMouseY - ClientRect.top;
        }
        
        f32 LastFrameTimeInS = LastFrameTimeInMS / 1000.0f;
        //TODO(Chen): update & render here
        UpdateAndRender(GameMemory, GameMemorySize, gWindowWidth, gWindowHeight, &Input, LastFrameTimeInS);
        f32 ProcTimeInMS = Win32GetTimeElapsedInMS(BeginCounter, Win32GetPerformanceCounter());
        SwapBuffers(WindowDC);
        f32 ElapsedTimeInMS = Win32GetTimeElapsedInMS(BeginCounter, Win32GetPerformanceCounter());
        
        if (ElapsedTimeInMS < DesiredFrameTimeInMS)
        {
            Sleep((LONG)(DesiredFrameTimeInMS - ElapsedTimeInMS));
            do 
            {
                ElapsedTimeInMS = Win32GetTimeElapsedInMS(BeginCounter, Win32GetPerformanceCounter());
            }
            while (ElapsedTimeInMS < DesiredFrameTimeInMS);
        }
        else
        {
            Sleep(2); //sleep anyways
        }
        
        char Titlebar[100];
        snprintf(Titlebar, sizeof(Titlebar), "Game Jam, Proc Time: %.2fms, Frame Time: %.2fms",
                 ProcTimeInMS, ElapsedTimeInMS);
        SetWindowText(Window, Titlebar);
        
        LastFrameTimeInMS = ElapsedTimeInMS;
    }
    
    return 0;
}
