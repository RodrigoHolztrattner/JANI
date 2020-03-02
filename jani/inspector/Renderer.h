////////////////////////////////////////////////////////////////////////////////
// Filename: JaniWorker.h
////////////////////////////////////////////////////////////////////////////////
#pragma once

//////////////
// INCLUDES //
//////////////
#include "JaniConfig.h"

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <stdio.h>
//#include <GL/gl3w.h>    // This example is using gl3w to access OpenGL functions. You may freely use any other OpenGL loader such as: glew, glad, glLoadGen, etc.
//#include <glew.h>
#include "glad.h"
#include <GLFW/glfw3.h>

///////////////
// NAMESPACE //
///////////////

// Jani
JaniNamespaceBegin(Jani)

// Inspector
JaniNamespaceBegin(Inspector)

////////////////////////////////////////////////////////////////////////////////
// Class name: Runtime
////////////////////////////////////////////////////////////////////////////////
class Renderer
{

//////////////////////////
public: // CONSTRUCTORS //
//////////////////////////

    Renderer();
    ~Renderer();
    
//////////////////////////
public: // MAIN METHODS //
//////////////////////////

    /*
    * Initialize the renderer
    */
    bool Initialize(uint32_t _window_width, uint32_t _window_height);

    /*
    * Begin the render frame
    */
    bool BeginRenderFrame();

    /*
    * End the render frame
    */
    void EndRenderFrame();

////////////////////////
private: // VARIABLES //
////////////////////////

    GLFWwindow* m_window = nullptr;

};

// Inspector
JaniNamespaceEnd(Inspector)

// Jani
JaniNamespaceEnd(Jani)