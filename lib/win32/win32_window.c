//========================================================================
// GLFW - An OpenGL framework
// File:        win32_window.c
// Platform:    Windows
// API version: 2.7
// WWW:         http://glfw.sourceforge.net
//------------------------------------------------------------------------
// Copyright (c) 2002-2006 Camilla Berglund
//
// This software is provided 'as-is', without any express or implied
// warranty. In no event will the authors be held liable for any damages
// arising from the use of this software.
//
// Permission is granted to anyone to use this software for any purpose,
// including commercial applications, and to alter it and redistribute it
// freely, subject to the following restrictions:
//
// 1. The origin of this software must not be misrepresented; you must not
//    claim that you wrote the original software. If you use this software
//    in a product, an acknowledgment in the product documentation would
//    be appreciated but is not required.
//
// 2. Altered source versions must be plainly marked as such, and must not
//    be misrepresented as being the original software.
//
// 3. This notice may not be removed or altered from any source
//    distribution.
//
//========================================================================

#include "internal.h"



//************************************************************************
//****                  GLFW internal functions                       ****
//************************************************************************

#define _GLFW_WNDCLASSNAME "GLFW27"


//========================================================================
// Enable/disable minimize/restore animations
//========================================================================

static int setMinMaxAnimations( int enable )
{
    ANIMATIONINFO AI;
    int old_enable;

    // Get old animation setting
    AI.cbSize = sizeof( ANIMATIONINFO );
    SystemParametersInfo( SPI_GETANIMATION, AI.cbSize, &AI, 0 );
    old_enable = AI.iMinAnimate;

    // If requested, change setting
    if( old_enable != enable )
    {
        AI.iMinAnimate = enable;
        SystemParametersInfo( SPI_SETANIMATION, AI.cbSize, &AI,
                              SPIF_SENDCHANGE );
    }

    return old_enable;
}


//========================================================================
// Function for bringing a window into focus and placing it on top of the
// window z stack. Due to some nastiness with how Win98/ME/2k/XP handles
// SetForegroundWindow, we have to go through some really bizarre measures to
// achieve this (thanks again, MS, for making life so much easier)!
//========================================================================

static void setForegroundWindow( HWND hWnd )
{
    int try_count = 0;
    int old_animate;

    // Try the standard approach first...
    BringWindowToTop( hWnd );
    SetForegroundWindow( hWnd );

    // If it worked, return now
    if( hWnd == GetForegroundWindow() )
    {
        // Try to modify the system settings (since this is the foreground
        // process, we are allowed to do this)
        SystemParametersInfo( SPI_SETFOREGROUNDLOCKTIMEOUT, 0, (LPVOID)0,
                              SPIF_SENDCHANGE );
        return;
    }

    // For other Windows versions than 95 & NT4.0, the standard approach
    // may not work, so if we failed we have to "trick" Windows into
    // making our window the foureground window: Iconify and restore
    // again. It is ugly, but it seems to work (we turn off those annoying
    // zoom animations to make it look a bit better at least).

    // Turn off minimize/restore animations
    old_animate = setMinMaxAnimations( 0 );

    // We try this a few times, just to be on the safe side of things...
    do
    {
        // Iconify & restore
        ShowWindow( hWnd, SW_HIDE );
        ShowWindow( hWnd, SW_SHOWMINIMIZED );
        ShowWindow( hWnd, SW_SHOWNORMAL );

        // Try to get focus
        BringWindowToTop( hWnd );
        SetForegroundWindow( hWnd );

        // We do not want to keep going on forever, so we keep track of
        // how many times we tried
        try_count ++;
    }
    while( hWnd != GetForegroundWindow() && try_count <= 3 );

    // Restore the system minimize/restore animation setting
    (void) setMinMaxAnimations( old_animate );

    // Try to modify the system settings (since this is now hopefully the
    // foreground process, we are probably allowed to do this)
    SystemParametersInfo( SPI_SETFOREGROUNDLOCKTIMEOUT, 0, (LPVOID)0,
                          SPIF_SENDCHANGE );
}


//========================================================================
// Returns the specified attribute of the specified pixel format
// NOTE: Do not call this unless we have found WGL_ARB_pixel_format
//========================================================================

static int getPixelFormatAttrib(int pixelformat, int attrib)
{
    int value;

    if( !_glfwWin.GetPixelFormatAttribiv( _glfwWin.DC, 0, 0, 1, &attrib, &value) )
    {
        // NOTE: We should probably handle this error somehow
        return 0;
    }

    return value;
}


//========================================================================
// Return a list of available and usable framebuffer configs
//========================================================================

static _GLFWfbconfig *getFBConfigs( unsigned int *found )
{
    _GLFWfbconfig *result;
    PIXELFORMATDESCRIPTOR pfd;
    int i, count;

    *found = 0;

    if( _glfwWin.GetPixelFormatAttribiv )
    {
        count = getPixelFormatAttrib( 1, WGL_NUMBER_PIXEL_FORMATS_ARB );
    }
    else
    {
        count = _glfw_DescribePixelFormat( _glfwWin.DC, 1, sizeof( PIXELFORMATDESCRIPTOR ), NULL );
    }

    if( !count )
    {
        return NULL;
    }

    result = (_GLFWfbconfig*) malloc( sizeof( _GLFWfbconfig ) * count );

    for( i = 1;  i <= count;  i++ )
    {
        if( _glfwWin.GetPixelFormatAttribiv )
        {
            // Get pixel format attributes through WGL_ARB_pixel_format

            if( !getPixelFormatAttrib( i, WGL_SUPPORT_OPENGL_ARB ) ||
                !getPixelFormatAttrib( i, WGL_DRAW_TO_WINDOW_ARB ) ||
                !getPixelFormatAttrib( i, WGL_DOUBLE_BUFFER_ARB ) )
            {
                // Only consider doublebuffered OpenGL pixel formats for windows
                continue;
            }

            if( getPixelFormatAttrib( i, WGL_PIXEL_TYPE_ARB ) != WGL_TYPE_RGBA_ARB )
            {
                // Only consider RGBA pixel formats
                continue;
            }

            result[*found].redBits = getPixelFormatAttrib( i, WGL_RED_BITS_ARB );
            result[*found].greenBits = getPixelFormatAttrib( i, WGL_GREEN_BITS_ARB );
            result[*found].blueBits = getPixelFormatAttrib( i, WGL_BLUE_BITS_ARB );
            result[*found].alphaBits = getPixelFormatAttrib( i, WGL_ALPHA_BITS_ARB );

            result[*found].depthBits = getPixelFormatAttrib( i, WGL_DEPTH_BITS_ARB );
            result[*found].stencilBits = getPixelFormatAttrib( i, WGL_STENCIL_BITS_ARB );

            result[*found].accumRedBits = getPixelFormatAttrib( i, WGL_ACCUM_RED_BITS_ARB );
            result[*found].accumGreenBits = getPixelFormatAttrib( i, WGL_ACCUM_GREEN_BITS_ARB );
            result[*found].accumBlueBits = getPixelFormatAttrib( i, WGL_ACCUM_BLUE_BITS_ARB );
            result[*found].accumAlphaBits = getPixelFormatAttrib( i, WGL_ACCUM_ALPHA_BITS_ARB );

            result[*found].auxBuffers = getPixelFormatAttrib( i, WGL_AUX_BUFFERS_ARB );
            result[*found].stereo = getPixelFormatAttrib( i, WGL_STEREO_ARB );
            result[*found].samples = getPixelFormatAttrib( i, WGL_SAMPLES_ARB );
        }
        else
        {
            // Get pixel format attributes through old-fashioned PFDs

            if( !_glfw_DescribePixelFormat( _glfwWin.DC, i, sizeof( PIXELFORMATDESCRIPTOR ), &pfd ) )
            {
                continue;
            }

            if( !( pfd.dwFlags & PFD_DRAW_TO_WINDOW ) ||
                !( pfd.dwFlags & PFD_SUPPORT_OPENGL ) ||
                !( pfd.dwFlags & PFD_DOUBLEBUFFER ) )
            {
                // Only consider doublebuffered OpenGL pixel formats for windows
                continue;
            }

            if( pfd.iPixelType != PFD_TYPE_RGBA )
            {
                // Only RGBA pixel formats considered
                continue;
            }

            result[*found].redBits = pfd.cRedBits;
            result[*found].greenBits = pfd.cGreenBits;
            result[*found].blueBits = pfd.cBlueBits;
            result[*found].alphaBits = pfd.cAlphaBits;

            result[*found].depthBits = pfd.cDepthBits;
            result[*found].stencilBits = pfd.cStencilBits;

            result[*found].accumRedBits = pfd.cAccumRedBits;
            result[*found].accumGreenBits = pfd.cAccumGreenBits;
            result[*found].accumBlueBits = pfd.cAccumBlueBits;
            result[*found].accumAlphaBits = pfd.cAccumAlphaBits;

            result[*found].auxBuffers = pfd.cAuxBuffers;
            result[*found].stereo = ( pfd.dwFlags & PFD_STEREO ) ? GL_TRUE : GL_FALSE;

            // PFD pixel formats do not support FSAA
            result[*found].samples = 0;
        }

        result[*found].platformID = i;

        (*found)++;
    }

    return result;
}


//========================================================================
// Creates an OpenGL context on the specified device context
//========================================================================

static HGLRC createContext( HDC dc, const _GLFWhints* hints, int pixelFormat )
{
    PIXELFORMATDESCRIPTOR pfd;

    if( !_glfw_DescribePixelFormat( dc, pixelFormat, sizeof(pfd), &pfd ) )
    {
        return NULL;
    }

    if( !_glfw_SetPixelFormat( dc, pixelFormat, &pfd ) )
    {
        return NULL;
    }

    // Static array sizes are bad; don't use them
    int i = 0, attribs[7];

    if( _glfwWin.CreateContextAttribsARB )
    {
        // Use the newer wglCreateContextAttribsARB

        if( hints->glMajor != 1 || hints->glMinor != 1 )
        {
            // Request an explicitly versioned context

            attribs[i++] = WGL_CONTEXT_MAJOR_VERSION_ARB;
            attribs[i++] = hints->glMajor;
            attribs[i++] = WGL_CONTEXT_MINOR_VERSION_ARB;
            attribs[i++] = hints->glMinor;
        }

        if( hints->glForward )
        {
            // Request a forward-compatible context

            attribs[i++] = WGL_CONTEXT_FLAGS_ARB;
            attribs[i++] = WGL_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB;
        }

        attribs[i++] = 0;

        return _glfwWin.CreateContextAttribsARB( dc, NULL, attribs );
    }

    return wglCreateContext( dc );
}


//========================================================================
// Translates a Windows key to the corresponding GLFW key
//========================================================================

static int translateKey( WPARAM wParam, LPARAM lParam )
{
    MSG next_msg;
    DWORD msg_time;
    DWORD scan_code;

    // Check which key was pressed or released
    switch( wParam )
    {
        // The SHIFT keys require special handling
        case VK_SHIFT:
        {
            // Compare scan code for this key with that of VK_RSHIFT in
            // order to determine which shift key was pressed (left or
            // right)
            scan_code = MapVirtualKey( VK_RSHIFT, 0 );
            if( ((lParam & 0x01ff0000) >> 16) == scan_code )
            {
                return GLFW_KEY_RSHIFT;
            }

            return GLFW_KEY_LSHIFT;
        }

        // The CTRL keys require special handling
        case VK_CONTROL:
        {
            // Is this an extended key (i.e. right key)?
            if( lParam & 0x01000000 )
            {
                return GLFW_KEY_RCTRL;
            }

            // Here is a trick: "Alt Gr" sends LCTRL, then RALT. We only
            // want the RALT message, so we try to see if the next message
            // is a RALT message. In that case, this is a false LCTRL!
            msg_time = GetMessageTime();
            if( PeekMessage( &next_msg, NULL, 0, 0, PM_NOREMOVE ) )
            {
                if( next_msg.message == WM_KEYDOWN ||
                    next_msg.message == WM_SYSKEYDOWN )
                {
                    if( next_msg.wParam == VK_MENU &&
                        (next_msg.lParam & 0x01000000) &&
                        next_msg.time == msg_time )
                    {
                        // Next message is a RALT down message, which
                        // means that this is NOT a proper LCTRL message!
                        return GLFW_KEY_UNKNOWN;
                    }
                }
            }

            return GLFW_KEY_LCTRL;
        }

        // The ALT keys require special handling
        case VK_MENU:
        {
            // Is this an extended key (i.e. right key)?
            if( lParam & 0x01000000 )
            {
                return GLFW_KEY_RALT;
            }

            return GLFW_KEY_LALT;
        }

        // The ENTER keys require special handling
        case VK_RETURN:
        {
            // Is this an extended key (i.e. right key)?
            if( lParam & 0x01000000 )
            {
                return GLFW_KEY_KP_ENTER;
            }

            return GLFW_KEY_ENTER;
        }

        // Special keys (non character keys)
        case VK_ESCAPE:        return GLFW_KEY_ESC;
        case VK_TAB:           return GLFW_KEY_TAB;
        case VK_BACK:          return GLFW_KEY_BACKSPACE;
        case VK_HOME:          return GLFW_KEY_HOME;
        case VK_END:           return GLFW_KEY_END;
        case VK_PRIOR:         return GLFW_KEY_PAGEUP;
        case VK_NEXT:          return GLFW_KEY_PAGEDOWN;
        case VK_INSERT:        return GLFW_KEY_INSERT;
        case VK_DELETE:        return GLFW_KEY_DEL;
        case VK_LEFT:          return GLFW_KEY_LEFT;
        case VK_UP:            return GLFW_KEY_UP;
        case VK_RIGHT:         return GLFW_KEY_RIGHT;
        case VK_DOWN:          return GLFW_KEY_DOWN;
        case VK_F1:            return GLFW_KEY_F1;
        case VK_F2:            return GLFW_KEY_F2;
        case VK_F3:            return GLFW_KEY_F3;
        case VK_F4:            return GLFW_KEY_F4;
        case VK_F5:            return GLFW_KEY_F5;
        case VK_F6:            return GLFW_KEY_F6;
        case VK_F7:            return GLFW_KEY_F7;
        case VK_F8:            return GLFW_KEY_F8;
        case VK_F9:            return GLFW_KEY_F9;
        case VK_F10:           return GLFW_KEY_F10;
        case VK_F11:           return GLFW_KEY_F11;
        case VK_F12:           return GLFW_KEY_F12;
        case VK_F13:           return GLFW_KEY_F13;
        case VK_F14:           return GLFW_KEY_F14;
        case VK_F15:           return GLFW_KEY_F15;
        case VK_F16:           return GLFW_KEY_F16;
        case VK_F17:           return GLFW_KEY_F17;
        case VK_F18:           return GLFW_KEY_F18;
        case VK_F19:           return GLFW_KEY_F19;
        case VK_F20:           return GLFW_KEY_F20;
        case VK_F21:           return GLFW_KEY_F21;
        case VK_F22:           return GLFW_KEY_F22;
        case VK_F23:           return GLFW_KEY_F23;
        case VK_F24:           return GLFW_KEY_F24;
        case VK_SPACE:         return GLFW_KEY_SPACE;

        // Numeric keypad
        case VK_NUMPAD0:       return GLFW_KEY_KP_0;
        case VK_NUMPAD1:       return GLFW_KEY_KP_1;
        case VK_NUMPAD2:       return GLFW_KEY_KP_2;
        case VK_NUMPAD3:       return GLFW_KEY_KP_3;
        case VK_NUMPAD4:       return GLFW_KEY_KP_4;
        case VK_NUMPAD5:       return GLFW_KEY_KP_5;
        case VK_NUMPAD6:       return GLFW_KEY_KP_6;
        case VK_NUMPAD7:       return GLFW_KEY_KP_7;
        case VK_NUMPAD8:       return GLFW_KEY_KP_8;
        case VK_NUMPAD9:       return GLFW_KEY_KP_9;
        case VK_DIVIDE:        return GLFW_KEY_KP_DIVIDE;
        case VK_MULTIPLY:      return GLFW_KEY_KP_MULTIPLY;
        case VK_SUBTRACT:      return GLFW_KEY_KP_SUBTRACT;
        case VK_ADD:           return GLFW_KEY_KP_ADD;
        case VK_DECIMAL:       return GLFW_KEY_KP_DECIMAL;

        // The rest (should be printable keys)
        default:
        {
            // Convert to printable character (ISO-8859-1 or Unicode)
            wParam = MapVirtualKey( (UINT) wParam, 2 ) & 0x0000FFFF;

            // Make sure that the character is uppercase
            if( _glfwLibrary.Sys.HasUnicode )
            {
                wParam = (WPARAM) CharUpperW( (LPWSTR) wParam );
            }
            else
            {
                wParam = (WPARAM) CharUpperA( (LPSTR) wParam );
            }

            // Valid ISO-8859-1 character?
            if( (wParam >=  32 && wParam <= 126) ||
                (wParam >= 160 && wParam <= 255) )
            {
                return (int) wParam;
            }

            return GLFW_KEY_UNKNOWN;
        }
    }
}


//========================================================================
// Translates a windows key to Unicode
//========================================================================

static void translateChar( DWORD wParam, DWORD lParam, int action )
{
    BYTE  keyboard_state[ 256 ];
    UCHAR char_buf[ 10 ];
    WCHAR unicode_buf[ 10 ];
    UINT  scan_code;
    int   i, num_chars, unicode;

    // Get keyboard state
    GetKeyboardState( keyboard_state );

    // Derive scan code from lParam and action
    scan_code = (lParam & 0x01ff0000) >> 16;
    if( action == GLFW_RELEASE )
    {
        scan_code |= 0x8000000;
    }

    // Do we have Unicode support?
    if( _glfwLibrary.Sys.HasUnicode )
    {
        // Convert to Unicode
        num_chars = ToUnicode(
            wParam,          // virtual-key code
            scan_code,       // scan code
            keyboard_state,  // key-state array
            unicode_buf,     // buffer for translated key
            10,              // size of translated key buffer
            0                // active-menu flag
        );
        unicode = 1;
    }
    else
    {
        // Convert to ISO-8859-1
        num_chars = ToAscii(
            wParam,            // virtual-key code
            scan_code,         // scan code
            keyboard_state,    // key-state array
            (LPWORD) char_buf, // buffer for translated key
            0                  // active-menu flag
        );
        unicode = 0;
    }

    // Report characters
    for( i = 0;  i < num_chars;  i++ )
    {
        // Get next character from buffer
        if( unicode )
        {
            _glfwInputChar( (int) unicode_buf[ i ], action );
        }
        else
        {
            _glfwInputChar( (int) char_buf[ i ], action );
        }
    }
}


//========================================================================
// Window callback function (handles window events)
//========================================================================

static LRESULT CALLBACK windowProc( HWND hWnd, UINT uMsg,
                                    WPARAM wParam, LPARAM lParam )
{
    int WheelDelta, Iconified;

    // Handle certain window messages
    switch( uMsg )
    {
        // Window activate message? (iconification?)
        case WM_ACTIVATE:
        {
            _glfwWin.Active = LOWORD(wParam) != WA_INACTIVE ? GL_TRUE : GL_FALSE;

            Iconified = HIWORD(wParam) ? GL_TRUE : GL_FALSE;

            // Were we deactivated/iconified?
            if( (!_glfwWin.Active || Iconified) && !_glfwWin.Iconified )
            {
                _glfwInputDeactivation();

                // If we are in fullscreen mode we need to iconify
                if( _glfwWin.Opened && _glfwWin.Fullscreen )
                {
                    // Do we need to manually iconify?
                    if( !Iconified )
                    {
                        // Minimize window
                        CloseWindow( _glfwWin.Wnd );

                        // The window is now iconified
                        Iconified = GL_TRUE;
                    }

                    // Change display settings to the desktop resolution
                    ChangeDisplaySettings( NULL, CDS_FULLSCREEN );
                }

                // Unlock mouse
                if( !_glfwWin.OldMouseLockValid )
                {
                    _glfwWin.OldMouseLock = _glfwWin.MouseLock;
                    _glfwWin.OldMouseLockValid = GL_TRUE;
                    glfwEnable( GLFW_MOUSE_CURSOR );
                }
            }
            else if( _glfwWin.Active || !Iconified )
            {
                // If we are in fullscreen mode we need to maximize
                if( _glfwWin.Opened && _glfwWin.Fullscreen && _glfwWin.Iconified )
                {
                    // Change display settings to the user selected mode
                    _glfwSetVideoModeMODE( _glfwWin.ModeID );

                    // Do we need to manually restore window?
                    if( Iconified )
                    {
                        // Restore window
                        OpenIcon( _glfwWin.Wnd );

                        // The window is no longer iconified
                        Iconified = GL_FALSE;

                        // Activate window
                        ShowWindow( hWnd, SW_SHOW );
                        setForegroundWindow( _glfwWin.Wnd );
                        SetFocus( _glfwWin.Wnd );
                    }
                }

                // Lock mouse, if necessary
                if( _glfwWin.OldMouseLockValid && _glfwWin.OldMouseLock )
                {
                    glfwDisable( GLFW_MOUSE_CURSOR );
                }
                _glfwWin.OldMouseLockValid = GL_FALSE;
            }

            _glfwWin.Iconified = Iconified;
            return 0;
        }

        // Intercept system commands (forbid certain actions/events)
        case WM_SYSCOMMAND:
        {
            switch( wParam & 0xfff0 )
            {
                // Screensaver trying to start or monitor trying to enter
                // powersave?
                case SC_SCREENSAVE:
                case SC_MONITORPOWER:
                    if( _glfwWin.Fullscreen )
                    {
                        return 0;
                    }
                    else
                    {
                        break;
                    }

                // User trying to access application menu using ALT?
                case SC_KEYMENU:
                    return 0;
            }
            break;
        }

        // Did we receive a close message?
        case WM_CLOSE:
            PostQuitMessage( 0 );
            return 0;

        // Is a key being pressed?
        case WM_KEYDOWN:
        case WM_SYSKEYDOWN:
        {
            // Translate and report key press
            _glfwInputKey( translateKey( wParam, lParam ),
                           GLFW_PRESS );

            // Translate and report character input
            if( _glfwWin.CharCallback )
            {
                translateChar( (DWORD) wParam, (DWORD) lParam, GLFW_PRESS );
            }
            return 0;
          }  

        // Is a key being released?
        case WM_KEYUP:
        case WM_SYSKEYUP:
        {
            // Special trick: release both shift keys on SHIFT up event
            if( wParam == VK_SHIFT )
            {
                _glfwInputKey( GLFW_KEY_LSHIFT, GLFW_RELEASE );
                _glfwInputKey( GLFW_KEY_RSHIFT, GLFW_RELEASE );
            }
            else
            {
                // Translate and report key release
                _glfwInputKey( translateKey( wParam, lParam ),
                               GLFW_RELEASE );
            }

            // Translate and report character input
            if( _glfwWin.CharCallback )
            {
                translateChar( (DWORD) wParam, (DWORD) lParam, GLFW_RELEASE );
            }

            return 0;
        }

        // Were any of the mouse-buttons pressed?
        case WM_LBUTTONDOWN:
            SetCapture(hWnd);
            _glfwInputMouseClick( GLFW_MOUSE_BUTTON_LEFT, GLFW_PRESS );
            return 0;
        case WM_RBUTTONDOWN:
            SetCapture(hWnd);
            _glfwInputMouseClick( GLFW_MOUSE_BUTTON_RIGHT, GLFW_PRESS );
            return 0;
        case WM_MBUTTONDOWN:
            SetCapture(hWnd);
            _glfwInputMouseClick( GLFW_MOUSE_BUTTON_MIDDLE, GLFW_PRESS );
            return 0;
        case WM_XBUTTONDOWN:
        {
            if( HIWORD(wParam) == XBUTTON1 )
            {
                SetCapture(hWnd);
                _glfwInputMouseClick( GLFW_MOUSE_BUTTON_4, GLFW_PRESS );
            }
            else if( HIWORD(wParam) == XBUTTON2 )
            {
                SetCapture(hWnd);
                _glfwInputMouseClick( GLFW_MOUSE_BUTTON_5, GLFW_PRESS );
            }
            return 1;
        }

        // Were any of the mouse-buttons released?
        case WM_LBUTTONUP:
            ReleaseCapture();
            _glfwInputMouseClick( GLFW_MOUSE_BUTTON_LEFT, GLFW_RELEASE );
            return 0;
        case WM_RBUTTONUP:
            ReleaseCapture();
            _glfwInputMouseClick( GLFW_MOUSE_BUTTON_RIGHT, GLFW_RELEASE );
            return 0;
        case WM_MBUTTONUP:
            ReleaseCapture();
            _glfwInputMouseClick( GLFW_MOUSE_BUTTON_MIDDLE, GLFW_RELEASE );
            return 0;
        case WM_XBUTTONUP:
        {
            if( HIWORD(wParam) == XBUTTON1 )
            {
                ReleaseCapture();
                _glfwInputMouseClick( GLFW_MOUSE_BUTTON_4, GLFW_RELEASE );
            }
            else if( HIWORD(wParam) == XBUTTON2 )
            {
                ReleaseCapture();
                _glfwInputMouseClick( GLFW_MOUSE_BUTTON_5, GLFW_RELEASE );
            }
            return 1;
        }

        // Did the mouse move?
        case WM_MOUSEMOVE:
        {
            int NewMouseX, NewMouseY;

            // Get signed (!) mouse position
            NewMouseX = (int)((short)LOWORD(lParam));
            NewMouseY = (int)((short)HIWORD(lParam));

            if( NewMouseX != _glfwInput.OldMouseX ||
                NewMouseY != _glfwInput.OldMouseY )
            {
                if( _glfwWin.MouseLock )
                {
                    _glfwInput.MousePosX += NewMouseX -
                                            _glfwInput.OldMouseX;
                    _glfwInput.MousePosY += NewMouseY -
                                            _glfwInput.OldMouseY;
                }
                else
                {
                    _glfwInput.MousePosX = NewMouseX;
                    _glfwInput.MousePosY = NewMouseY;
                }
                _glfwInput.OldMouseX = NewMouseX;
                _glfwInput.OldMouseY = NewMouseY;
                _glfwInput.MouseMoved = GL_TRUE;

                // Call user callback function
                if( _glfwWin.MousePosCallback )
                {
                    _glfwWin.MousePosCallback( _glfwInput.MousePosX,
                                               _glfwInput.MousePosY );
                }
            }
            return 0;
        }

        // Mouse wheel action?
        case WM_MOUSEWHEEL:
        {
            // WM_MOUSEWHEEL is not supported under Windows 95
            if( _glfwLibrary.Sys.WinVer != _GLFW_WIN_95 )
            {
                WheelDelta = (((int)wParam) >> 16) / WHEEL_DELTA;
                _glfwInput.WheelPos += WheelDelta;
                if( _glfwWin.MouseWheelCallback )
                {
                    _glfwWin.MouseWheelCallback( _glfwInput.WheelPos );
                }
                return 0;
            }
            break;
        }

        // Resize the window?
        case WM_SIZE:
        {
            // get the new size
            _glfwWin.Width  = LOWORD(lParam);
            _glfwWin.Height = HIWORD(lParam);

            // If the mouse is locked, update the clipping rect
            if( _glfwWin.MouseLock )
            {
                RECT ClipWindowRect;
                if( GetWindowRect( _glfwWin.Wnd, &ClipWindowRect ) )
                {
                    ClipCursor( &ClipWindowRect );
                }
            }

            // Call the user-supplied callback, if it exists
            if( _glfwWin.WindowSizeCallback )
            {
                _glfwWin.WindowSizeCallback( LOWORD(lParam),
                                             HIWORD(lParam) );
            }
            return 0;
        }

        // Move the window?
        case WM_MOVE:
        {
            // If the mouse is locked, update the clipping rect
            if( _glfwWin.MouseLock )
            {
                RECT ClipWindowRect;
                if( GetWindowRect( _glfwWin.Wnd, &ClipWindowRect ) )
                {
                    ClipCursor( &ClipWindowRect );
                }
            }
            return 0;
        }

        // Was the window contents damaged?
        case WM_PAINT:
        {
            // Call user callback function
            if( _glfwWin.WindowRefreshCallback )
            {
                _glfwWin.WindowRefreshCallback();
            }
            break;
        }

        case WM_DISPLAYCHANGE:
        {
            // TODO: Do stuff here.

            break;
        }
    }

    // Pass all unhandled messages to DefWindowProc
    return DefWindowProc( hWnd, uMsg, wParam, lParam );
}


//========================================================================
// Translate client window size to full window size (including window borders)
//========================================================================

static void getFullWindowSize( int clientWidth, int clientHeight,
                               int *fullWidth, int *fullHeight )
{
    RECT rect;

    // Create a window rectangle
    rect.left   = (long)0;
    rect.right  = (long)clientWidth - 1;
    rect.top    = (long)0;
    rect.bottom = (long)clientHeight - 1;

    // Adjust according to window styles
    AdjustWindowRectEx( &rect, _glfwWin.dwStyle, FALSE, _glfwWin.dwExStyle );

    // Calculate width and height of full window
    *fullWidth = rect.right - rect.left + 1;
    *fullHeight = rect.bottom - rect.top + 1;
}


//========================================================================
// Initialize WGL-specific extensions
//========================================================================

static void initWGLExtensions( void )
{
    _glfwWin.GetExtensionsStringEXT = (WGLGETEXTENSIONSSTRINGEXT_T)
        wglGetProcAddress( "wglGetExtensionsStringEXT" );
    if( !_glfwWin.GetExtensionsStringEXT )
    {
        _glfwWin.GetExtensionsStringARB = (WGLGETEXTENSIONSSTRINGARB_T)
            wglGetProcAddress( "wglGetExtensionsStringARB" );
        if( !_glfwWin.GetExtensionsStringARB )
        {
            return;
        }
    }

    if( _glfwPlatformExtensionSupported( "WGL_EXT_swap_control" ) )
    {
        _glfwWin.CreateContextAttribsARB = (WGLCREATECONTEXTATTRIBSARB_T)
            wglGetProcAddress( "wglCreateContextAttribsARB" );
    }
    else
    {
        _glfwWin.CreateContextAttribsARB = NULL;
    }

    if( _glfwPlatformExtensionSupported( "WGL_EXT_swap_control" ) )
    {
        _glfwWin.SwapInterval = (WGLSWAPINTERVALEXT_T)
            wglGetProcAddress( "wglSwapIntervalEXT" );
    }
    else
    {
        _glfwWin.SwapInterval = NULL;
    }
    
    if( _glfwPlatformExtensionSupported( "WGL_ARB_pixel_format" ) )
    {
        _glfwWin.ChoosePixelFormat = (WGLCHOOSEPIXELFORMATARB_T)
            wglGetProcAddress( "wglChoosePixelFormatARB" );
        _glfwWin.GetPixelFormatAttribiv = (WGLGETPIXELFORMATATTRIBIVARB_T)
            wglGetProcAddress( "wglGetPixelFormatAttribivARB" );
    }
    else
    {
        _glfwWin.ChoosePixelFormat = NULL;
        _glfwWin.GetPixelFormatAttribiv = NULL;
    }  
}


//========================================================================
// Registers the GLFW window class
//========================================================================

static ATOM registerWindowClass( void )
{
    WNDCLASS wc;

    // Set window class parameters
    wc.style         = CS_HREDRAW | CS_VREDRAW | CS_OWNDC; // Redraw on...
    wc.lpfnWndProc   = (WNDPROC)windowProc;           // Message handler
    wc.cbClsExtra    = 0;                             // No extra class data
    wc.cbWndExtra    = 0;                             // No extra window data
    wc.hInstance     = _glfwLibrary.Instance;         // Set instance
    wc.hCursor       = LoadCursor( NULL, IDC_ARROW ); // Load arrow pointer
    wc.hbrBackground = NULL;                          // No background
    wc.lpszMenuName  = NULL;                          // No menu
    wc.lpszClassName = _GLFW_WNDCLASSNAME;            // Set class name

    // Load user-provided icon if available
    wc.hIcon = LoadIcon( _glfwLibrary.Instance, "GLFW_ICON" );
    if( !wc.hIcon )
    {
        // Load default icon
        wc.hIcon = LoadIcon( NULL, IDI_WINLOGO ); 
    }

    return RegisterClass( &wc );
}


//========================================================================
// Returns the closest matching pixel format, or zero on error
//========================================================================

static int choosePixelFormat( const _GLFWfbconfig *fbconfig )
{
    unsigned int fbcount;
    int pixelFormat;
    _GLFWfbconfig *fbconfigs;
    const _GLFWfbconfig *closest;

    fbconfigs = getFBConfigs( &fbcount );
    if( !fbconfigs )
    {
        return 0;
    }

    closest = _glfwChooseFBConfig( fbconfig, fbconfigs, fbcount );
    if( !closest )
    {
        free( fbconfigs );
        return 0;
    }

    pixelFormat = (int) closest->platformID;

    free( fbconfigs );
    fbconfigs = NULL;

    return pixelFormat;
}


//========================================================================
// Creates the GLFW window and rendering context
//========================================================================

static int createWindow( int mode, const _GLFWhints *hints, const _GLFWfbconfig *fbconfig )
{
    DWORD dwStyle, dwExStyle;
    int pixelFormat, fullWidth, fullHeight;
    RECT wa;
    POINT pos;

    _glfwWin.DC  = NULL;
    _glfwWin.RC  = NULL;
    _glfwWin.Wnd = NULL;

    // Set common window styles
    dwStyle   = WS_CLIPSIBLINGS | WS_CLIPCHILDREN | WS_VISIBLE;
    dwExStyle = WS_EX_APPWINDOW;

    // Set window style, depending on fullscreen mode
    if( _glfwWin.Fullscreen )
    {
        dwStyle |= WS_POPUP;

        // Here's a trick for helping us getting window focus
        // (SetForegroundWindow doesn't work properly under
        // Win98/ME/2K/.NET/+)
        /*
        if( _glfwLibrary.Sys.WinVer != _GLFW_WIN_95 &&
            _glfwLibrary.Sys.WinVer != _GLFW_WIN_NT4 && 
            _glfwLibrary.Sys.WinVer != _GLFW_WIN_XP )
        {
            dwStyle |= WS_MINIMIZE;
        }
        */
    }
    else
    {
        dwStyle |= WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;

        if( !hints->windowNoResize )
        {
            dwStyle |= ( WS_MAXIMIZEBOX | WS_SIZEBOX );
            dwExStyle |= WS_EX_WINDOWEDGE;
        }
    }

    // Remember window styles (used by getFullWindowSize)
    _glfwWin.dwStyle   = dwStyle;
    _glfwWin.dwExStyle = dwExStyle;

    // Set window size to true requested size (adjust for window borders)
    getFullWindowSize( _glfwWin.Width, _glfwWin.Height, &fullWidth, &fullHeight );

    // Adjust window position to working area (e.g. if the task bar is at
    // the top of the display). Fullscreen windows are always opened in
    // the upper left corner regardless of the desktop working area. 
    if( _glfwWin.Fullscreen )
    {
        wa.left = wa.top = 0;
    }
    else
    {
        SystemParametersInfo( SPI_GETWORKAREA, 0, &wa, 0 );
    }

    _glfwWin.Wnd = CreateWindowEx(
               _glfwWin.dwExStyle,        // Extended style
               _GLFW_WNDCLASSNAME,        // Class name
               "GLFW Window",             // Window title
               _glfwWin.dwStyle,          // Defined window style
               wa.left, wa.top,           // Window position
               fullWidth,                 // Decorated window width
               fullHeight,                // Decorated window height
               NULL,                      // No parent window
               NULL,                      // No menu
               _glfwLibrary.Instance,     // Instance
               NULL );                    // Nothing to WM_CREATE

    if( !_glfwWin.Wnd )
    {
        return GL_FALSE;
    }

    _glfwWin.DC = GetDC( _glfwWin.Wnd );
    if( !_glfwWin.DC )
    {
        return GL_FALSE;
    }

    pixelFormat = choosePixelFormat( fbconfig );
    if( !pixelFormat )
    {
        return GL_FALSE;
    }

    _glfwWin.RC = createContext( _glfwWin.DC, hints, pixelFormat );
    if( !_glfwWin.RC )
    {
        return GL_FALSE;
    }

    if( !wglMakeCurrent( _glfwWin.DC, _glfwWin.RC ) )
    {
        return GL_FALSE;
    }

    initWGLExtensions();

    // Initialize mouse position data
    GetCursorPos( &pos );
    ScreenToClient( _glfwWin.Wnd, &pos );
    _glfwInput.OldMouseX = _glfwInput.MousePosX = pos.x;
    _glfwInput.OldMouseY = _glfwInput.MousePosY = pos.y;

    return GL_TRUE;
}


//========================================================================
// Destroys the GLFW window and rendering context
//========================================================================

static void destroyWindow( void )
{
    // Do we have a rendering context?
    if( _glfwWin.RC )
    {
        // Release the DC and RC contexts
        wglMakeCurrent( NULL, NULL );

        // Delete the rendering context
        wglDeleteContext( _glfwWin.RC );
        _glfwWin.RC = NULL;
    }

    // Do we have a device context?
    if( _glfwWin.DC )
    {
        // Release the device context
        ReleaseDC( _glfwWin.Wnd, _glfwWin.DC );
        _glfwWin.DC = NULL;
    }

    // Do we have a window?
    if( _glfwWin.Wnd )
    {
        // Destroy the window
        if( _glfwLibrary.Sys.WinVer <= _GLFW_WIN_NT4 )
        {
            // Note: Hiding the window first fixes an annoying W98/NT4
            // remaining icon bug for fullscreen displays
            ShowWindow( _glfwWin.Wnd, SW_HIDE );
        }

        DestroyWindow( _glfwWin.Wnd );
        _glfwWin.Wnd = NULL;
    }
}



//************************************************************************
//****               Platform implementation functions                ****
//************************************************************************

//========================================================================
// Here is where the window is created, and the OpenGL rendering context is
// created
//========================================================================

int _glfwPlatformOpenWindow( int width, int height, int mode,
                             const _GLFWhints *hints,
                             const _GLFWfbconfig *fbconfig )
{
    int recreateContext = GL_FALSE;

    // Clear platform specific GLFW window state
    _glfwWin.ClassAtom         = 0;
    _glfwWin.OldMouseLockValid = GL_FALSE;
    _glfwWin.ChoosePixelFormat = NULL;
    _glfwWin.GetPixelFormatAttribiv = NULL;
    _glfwWin.GetExtensionsStringARB = NULL;
    _glfwWin.GetExtensionsStringEXT = NULL;
    _glfwWin.CreateContextAttribsARB = NULL;

    _glfwWin.DesiredRefreshRate = hints->refreshRate;

    _glfwWin.ClassAtom = registerWindowClass();
    if( !_glfwWin.ClassAtom )
    {
        _glfwPlatformCloseWindow();
        return GL_FALSE;
    }

    if( _glfwWin.Fullscreen )
    {
        _glfwSetVideoMode( &_glfwWin.Width, &_glfwWin.Height,
                           fbconfig->redBits, fbconfig->greenBits, fbconfig->blueBits,
                           hints->refreshRate );
    }

    if( !createWindow( mode, hints, fbconfig ) )
    {
        _glfwPlatformCloseWindow();
        return GL_FALSE;
    }

    if( hints->glMajor > 2 )
    {
        if( !_glfwWin.CreateContextAttribsARB )
        {
            _glfwPlatformCloseWindow();
            return GL_FALSE;
        }

        recreateContext = GL_TRUE;
    }

    if( hints->samples > 0 )
    {
        if( !_glfwWin.GetPixelFormatAttribiv )
        {
            recreateContext = GL_TRUE;
        }
    }

    if( recreateContext )
    {
        // Some window hints require us to re-create the context using
        // extensions retrieved through the current context
        // Yes, this is strange, and yes, this is the proper way on Win32

        // As Windows only allows you to set the pixel format once for a
        // window, we need to destroy the current window and create a new one
        // to be able to use the new pixel format

        destroyWindow();

        if( !createWindow( mode, hints, fbconfig ) )
        {
            _glfwPlatformCloseWindow();
            return GL_FALSE;
        }
    }

    // Make sure that our window ends up on top of things
    if( _glfwWin.Fullscreen )
    {
        // Place the window above all topmost windows
        SetWindowPos( _glfwWin.Wnd, HWND_TOPMOST, 0,0,0,0,
                      SWP_NOMOVE | SWP_NOSIZE );
    }

    setForegroundWindow( _glfwWin.Wnd );
    SetFocus( _glfwWin.Wnd );

    // Start by clearing the front buffer to black (avoid ugly desktop
    // remains in our OpenGL window)
    glClear( GL_COLOR_BUFFER_BIT );
    _glfw_SwapBuffers( _glfwWin.DC );

    return GL_TRUE;
}


//========================================================================
// Properly kill the window / video display
//========================================================================

void _glfwPlatformCloseWindow( void )
{
    destroyWindow();

    // Do we have an instance?
    if( _glfwWin.ClassAtom )
    {
        // Unregister class
        UnregisterClass( _GLFW_WNDCLASSNAME, _glfwLibrary.Instance );
        _glfwWin.ClassAtom = 0;
    }

    // Are we in fullscreen mode?
    if( _glfwWin.Fullscreen )
    {
        // Switch back to desktop resolution
        ChangeDisplaySettings( NULL, CDS_FULLSCREEN );
    }
}


//========================================================================
// Set the window title
//========================================================================

void _glfwPlatformSetWindowTitle( const char *title )
{
    // Set window title
    (void) SetWindowText( _glfwWin.Wnd, title );
}


//========================================================================
// Set the window size.
//========================================================================

void _glfwPlatformSetWindowSize( int width, int height )
{
    int     bpp, mode = 0, refresh;
    int     sizechanged = GL_FALSE;
    GLint   drawbuffer;
    GLfloat clearcolor[4];

    // If we are in fullscreen mode, get some info about the current mode
    if( _glfwWin.Fullscreen )
    {
        DEVMODE dm;

        // Get current BPP settings
        dm.dmSize = sizeof( DEVMODE );
        if( EnumDisplaySettings( NULL, _glfwWin.ModeID, &dm ) )
        {
            // Get bpp
            bpp = dm.dmBitsPerPel;

            // Get closest match for target video mode
            refresh = _glfwWin.DesiredRefreshRate;
            mode = _glfwGetClosestVideoModeBPP( &width, &height, &bpp,
                                                &refresh );
        }
        else
        {
            mode = _glfwWin.ModeID;
        }
    }
    else
    {
        // If we are in windowed mode, adjust the window size to
        // compensate for window decorations
        getFullWindowSize( width, height, &width, &height );
    }

    // Change window size before changing fullscreen mode?
    if( _glfwWin.Fullscreen && (width > _glfwWin.Width) )
    {
        SetWindowPos( _glfwWin.Wnd, HWND_TOP, 0, 0, width, height,
                      SWP_NOOWNERZORDER | SWP_NOMOVE | SWP_NOZORDER );
        sizechanged = GL_TRUE;
    }

    // Change fullscreen video mode?
    if( _glfwWin.Fullscreen && mode != _glfwWin.ModeID )
    {
        // Change video mode
        _glfwSetVideoModeMODE( mode );

        // Clear the front buffer to black (avoid ugly desktop remains in
        // our OpenGL window)
        glGetIntegerv( GL_DRAW_BUFFER, &drawbuffer );
        glGetFloatv( GL_COLOR_CLEAR_VALUE, clearcolor );
        glClearColor( 0.0f, 0.0f, 0.0f, 0.0f );
        glClear( GL_COLOR_BUFFER_BIT );
        if( drawbuffer == GL_BACK )
        {
            _glfw_SwapBuffers( _glfwWin.DC );
        }
        glClearColor( clearcolor[0], clearcolor[1], clearcolor[2],
                      clearcolor[3] );
    }

    // Set window size (if not already changed)
    if( !sizechanged )
    {
        SetWindowPos( _glfwWin.Wnd, HWND_TOP, 0, 0, width, height,
                      SWP_NOOWNERZORDER | SWP_NOMOVE | SWP_NOZORDER );
    }
}


//========================================================================
// Set the window position
//========================================================================

void _glfwPlatformSetWindowPos( int x, int y )
{
    // Set window position
    (void) SetWindowPos( _glfwWin.Wnd, HWND_TOP, x, y, 0, 0,
                         SWP_NOOWNERZORDER | SWP_NOSIZE | SWP_NOZORDER );
}


//========================================================================
// Window iconification
//========================================================================

void _glfwPlatformIconifyWindow( void )
{
    // Iconify window
    CloseWindow( _glfwWin.Wnd );

    // Window is now iconified
    _glfwWin.Iconified = GL_TRUE;

    // If we are in fullscreen mode we need to change video modes
    if( _glfwWin.Fullscreen )
    {
        // Change display settings to the desktop resolution
        ChangeDisplaySettings( NULL, CDS_FULLSCREEN );
    }

    // Unlock mouse
    if( !_glfwWin.OldMouseLockValid )
    {
        _glfwWin.OldMouseLock = _glfwWin.MouseLock;
        _glfwWin.OldMouseLockValid = GL_TRUE;
        glfwEnable( GLFW_MOUSE_CURSOR );
    }
}


//========================================================================
// Window un-iconification
//========================================================================

void _glfwPlatformRestoreWindow( void )
{
    // If we are in fullscreen mode we need to change video modes
    if( _glfwWin.Fullscreen )
    {
        // Change display settings to the user selected mode
        _glfwSetVideoModeMODE( _glfwWin.ModeID );
    }

    // Un-iconify window
    OpenIcon( _glfwWin.Wnd );

    // Make sure that our window ends up on top of things
    ShowWindow( _glfwWin.Wnd, SW_SHOW );
    setForegroundWindow( _glfwWin.Wnd );
    SetFocus( _glfwWin.Wnd );

    // Window is no longer iconified
    _glfwWin.Iconified = GL_FALSE;

    // Lock mouse, if necessary
    if( _glfwWin.OldMouseLockValid && _glfwWin.OldMouseLock )
    {
        glfwDisable( GLFW_MOUSE_CURSOR );
    }
    _glfwWin.OldMouseLockValid = GL_FALSE;
}


//========================================================================
// Swap buffers (double-buffering)
//========================================================================

void _glfwPlatformSwapBuffers( void )
{
    _glfw_SwapBuffers( _glfwWin.DC );
}


//========================================================================
// Set double buffering swap interval
//========================================================================

void _glfwPlatformSwapInterval( int interval )
{
    if( _glfwWin.SwapInterval )
    {
        _glfwWin.SwapInterval( interval );
    }
}


//========================================================================
// Write back window parameters into GLFW window structure
//========================================================================

void _glfwPlatformRefreshWindowParams( void )
{
    PIXELFORMATDESCRIPTOR pfd;
    DEVMODE dm;
    int     PixelFormat, mode;

    // Obtain a detailed description of current pixel format
    PixelFormat = _glfw_GetPixelFormat( _glfwWin.DC );

    if( !_glfwWin.GetPixelFormatAttribiv )
    {
        _glfw_DescribePixelFormat( _glfwWin.DC, PixelFormat,
                                   sizeof(PIXELFORMATDESCRIPTOR), &pfd );

        // Is current OpenGL context accelerated?
        _glfwWin.Accelerated = (pfd.dwFlags & PFD_GENERIC_ACCELERATED) ||
                               !(pfd.dwFlags & PFD_GENERIC_FORMAT) ? 1 : 0;

        // "Standard" window parameters
        _glfwWin.RedBits        = pfd.cRedBits;
        _glfwWin.GreenBits      = pfd.cGreenBits;
        _glfwWin.BlueBits       = pfd.cBlueBits;
        _glfwWin.AlphaBits      = pfd.cAlphaBits;
        _glfwWin.DepthBits      = pfd.cDepthBits;
        _glfwWin.StencilBits    = pfd.cStencilBits;
        _glfwWin.AccumRedBits   = pfd.cAccumRedBits;
        _glfwWin.AccumGreenBits = pfd.cAccumGreenBits;
        _glfwWin.AccumBlueBits  = pfd.cAccumBlueBits;
        _glfwWin.AccumAlphaBits = pfd.cAccumAlphaBits;
        _glfwWin.AuxBuffers     = pfd.cAuxBuffers;
        _glfwWin.Stereo         = pfd.dwFlags & PFD_STEREO ? 1 : 0;
        _glfwWin.Samples        = 0;
    }
    else
    {
        const int attribs[] = {
            WGL_ACCELERATION_ARB,
            WGL_RED_BITS_ARB,
            WGL_GREEN_BITS_ARB,
            WGL_BLUE_BITS_ARB,
            WGL_ALPHA_BITS_ARB,
            WGL_DEPTH_BITS_ARB,
            WGL_STENCIL_BITS_ARB,
            WGL_ACCUM_RED_BITS_ARB,
            WGL_ACCUM_GREEN_BITS_ARB,
            WGL_ACCUM_BLUE_BITS_ARB,
            WGL_ACCUM_ALPHA_BITS_ARB,
            WGL_AUX_BUFFERS_ARB,
            WGL_STEREO_ARB,
            WGL_SAMPLES_ARB
        };

        int values[sizeof(attribs) / sizeof(attribs[0])];

        _glfwWin.GetPixelFormatAttribiv( _glfwWin.DC, PixelFormat, 0, 
                                         sizeof(attribs) / sizeof(attribs[0]), 
                                         attribs, values);

        // Is current OpenGL context accelerated?
        _glfwWin.Accelerated = (values[0] == WGL_FULL_ACCELERATION_ARB);

        // "Standard" window parameters
        _glfwWin.RedBits        = values[1];
        _glfwWin.GreenBits      = values[2];
        _glfwWin.BlueBits       = values[3];
        _glfwWin.AlphaBits      = values[4];
        _glfwWin.DepthBits      = values[5];
        _glfwWin.StencilBits    = values[6];
        _glfwWin.AccumRedBits   = values[7];
        _glfwWin.AccumGreenBits = values[8]; 
        _glfwWin.AccumBlueBits  = values[9];
        _glfwWin.AccumAlphaBits = values[10];
        _glfwWin.AuxBuffers     = values[11];
        _glfwWin.Stereo         = values[12];
        _glfwWin.Samples        = values[13];
    }

    // Get refresh rate
    mode = _glfwWin.Fullscreen ? _glfwWin.ModeID : ENUM_CURRENT_SETTINGS;
    dm.dmSize = sizeof( DEVMODE );

    if( EnumDisplaySettings( NULL, mode, &dm ) )
    {
        _glfwWin.RefreshRate = dm.dmDisplayFrequency;
        if( _glfwWin.RefreshRate <= 1 )
        {
            _glfwWin.RefreshRate = 0;
        }
    }
    else
    {
        _glfwWin.RefreshRate = 0;
    }
}


//========================================================================
// Poll for new window and input events
//========================================================================

void _glfwPlatformPollEvents( void )
{
    MSG msg;
    int winclosed = GL_FALSE;

    // Flag: mouse was not moved (will be changed by _glfwGetNextEvent if
    // there was a mouse move event)
    _glfwInput.MouseMoved = GL_FALSE;
    if( _glfwWin.MouseLock )
    {
        _glfwInput.OldMouseX = _glfwWin.Width/2;
        _glfwInput.OldMouseY = _glfwWin.Height/2;
    }
    else
    {
        _glfwInput.OldMouseX = _glfwInput.MousePosX;
        _glfwInput.OldMouseY = _glfwInput.MousePosY;
    }

    // Check for new window messages
    while( PeekMessage( &msg, NULL, 0, 0, PM_REMOVE ) )
    {
        switch( msg.message )
        {
            // QUIT-message (from close window)?
            case WM_QUIT:
                winclosed = GL_TRUE;
                break;

            // Ok, send it to the window message handler
            default:
                DispatchMessage( &msg );
                break;
        }
    }

    // LSHIFT/RSHIFT fixup (keys tend to "stick" without this fix)
    // This is the only async event handling in GLFW, but it solves some
    // nasty problems.
    // Caveat: Does not work under Win 9x/ME.
    if( _glfwLibrary.Sys.WinVer >= _GLFW_WIN_NT4 )
    {
        int lshift_down, rshift_down;

        // Get current state of left and right shift keys
        lshift_down = (GetAsyncKeyState( VK_LSHIFT ) >> 15) & 1;
        rshift_down = (GetAsyncKeyState( VK_RSHIFT ) >> 15) & 1;

        // See if this differs from our belief of what has happened
        // (we only have to check for lost key up events)
        if( !lshift_down && _glfwInput.Key[ GLFW_KEY_LSHIFT ] == 1 )
        {
            _glfwInputKey( GLFW_KEY_LSHIFT, GLFW_RELEASE );
        }
        if( !rshift_down && _glfwInput.Key[ GLFW_KEY_RSHIFT ] == 1 )
        {
            _glfwInputKey( GLFW_KEY_RSHIFT, GLFW_RELEASE );
        }
    }

    // Did we have mouse movement in locked cursor mode?
    if( _glfwInput.MouseMoved && _glfwWin.MouseLock )
    {
        _glfwPlatformSetMouseCursorPos( _glfwWin.Width / 2,
                                        _glfwWin.Height / 2 );
    }

    // Was there a window close request?
    if( winclosed && _glfwWin.WindowCloseCallback )
    {
        // Check if the program wants us to close the window
        winclosed = _glfwWin.WindowCloseCallback();
    }
    if( winclosed )
    {
        glfwCloseWindow();
    }
}


//========================================================================
// _glfwPlatformWaitEvents() - Wait for new window and input events
//========================================================================

void _glfwPlatformWaitEvents( void )
{
    // Wait for new events
    WaitMessage();

    // Poll new events
    _glfwPlatformPollEvents();
}


//========================================================================
// Hide mouse cursor (lock it)
//========================================================================

void _glfwPlatformHideMouseCursor( void )
{
    RECT ClipWindowRect;

    // Hide cursor
    ShowCursor( FALSE );

    // Clip cursor to the window
    if( GetWindowRect( _glfwWin.Wnd, &ClipWindowRect ) )
    {
        ClipCursor( &ClipWindowRect );
    }

    // Capture cursor to user window
    SetCapture( _glfwWin.Wnd );
}


//========================================================================
// Show mouse cursor (unlock it)
//========================================================================

void _glfwPlatformShowMouseCursor( void )
{
    // Un-capture cursor
    ReleaseCapture();

    // Disable cursor clipping
    ClipCursor( NULL );

    // Show cursor
    ShowCursor( TRUE );
}


//========================================================================
// Set physical mouse cursor position
//========================================================================

void _glfwPlatformSetMouseCursorPos( int x, int y )
{
    POINT pos;

    // Convert client coordinates to screen coordinates
    pos.x = x;
    pos.y = y;
    ClientToScreen( _glfwWin.Wnd, &pos );

    // Change cursor position
    SetCursorPos( pos.x, pos.y );
}

