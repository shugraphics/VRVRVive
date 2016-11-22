#include "GLTools.h"

static bool g_OPENGL_initialized = false;
static bool g_GLFW_initialized = false;
static bool g_SDL_initialized = false;
static glm::vec2 g_mainWindowSize = glm::vec2(0,0);

void initOpenGL() {
	if (!g_OPENGL_initialized)
	{
		glewExperimental = GL_TRUE;
		GLenum nGlewError = glewInit();
		if (nGlewError != GLEW_OK)
		{
			printf( "%s - Error initializing GLEW! %s\n", __FUNCTION__, glewGetErrorString( nGlewError ) );
		}
		glGetError(); // to clear the error caused deep in GLEW

		glEnable(GL_DEPTH_TEST);
		g_OPENGL_initialized = true;
	}
}

void initSDL() {
	if (SDL_Init(SDL_INIT_EVERYTHING) != 0)
    {
        printf("Error: %s\n", SDL_GetError());
    }
}

void initGLFW() {
	if (g_GLFW_initialized == false)
	{
		glfwInit();

		#ifdef __APPLE__
		glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
		glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
		glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
		glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

		glewExperimental = GL_TRUE;
		#endif
	}
}


GLFWwindow* generateWindow(int width, int height, int posX, int posY) {
	if (g_GLFW_initialized == false)
	{
		initGLFW();
	}
	
	GLFWwindow* window = glfwCreateWindow(width, height, "OpenGL Window", NULL, NULL);
	glfwSetWindowPos(window, posX, posY);
	glfwSetWindowSize(window, width, height);
	glfwMakeContextCurrent(window);

	OPENGLCONTEXT->setViewport(0,0,width,height);

	if (g_OPENGL_initialized == false)
	{
		initOpenGL();
		if (g_mainWindowSize == glm::vec2(0,0))
		{
			g_mainWindowSize = glm::vec2(width,height);
		}
	}

//	registerDefaultGLFWCallbacks(window);

	OPENGLCONTEXT->updateCache();

	return window;
}

static SDL_GLContext g_sdl_glcontext;

SDL_Window* generateWindow_SDL(int width, int height, int posX, int posY, Uint32 unWindowFlags) {

	if(!g_SDL_initialized)
	{
		initSDL();
	}

	SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);

	SDL_GL_SetAttribute( SDL_GL_CONTEXT_MAJOR_VERSION, 4 );
	SDL_GL_SetAttribute( SDL_GL_CONTEXT_MINOR_VERSION, 1 );

    //SDL_DisplayMode current;
    //SDL_GetCurrentDisplayMode(0, &current);
    SDL_Window *window = SDL_CreateWindow("ImGui SDL2+OpenGL3 example", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1280, 720, unWindowFlags);


	g_sdl_glcontext = SDL_GL_CreateContext(window);
	if (!g_sdl_glcontext)
	{
		//printf("Error: %s\n", SDL_GetError());
		cout<<SDL_GetError();
	}
	
	OPENGLCONTEXT->setViewport(0,0,width,height);
	if (g_OPENGL_initialized == false)
	{
		initOpenGL();
		if (g_mainWindowSize == glm::vec2(0,0))
		{
			g_mainWindowSize = glm::vec2(width,height);
		}
	}
	
	OPENGLCONTEXT->updateCache();

	return window;
}

glm::vec2 getMainWindowResolution()
{
	return g_mainWindowSize;
}

bool shouldClose(GLFWwindow* window)
{
	return (glfwWindowShouldClose(window) != 0);
}

void swapBuffers(GLFWwindow* window)
{
	glfwSwapBuffers(window);
    glfwPollEvents();
}

void destroyWindow(GLFWwindow* window)
{
	glfwDestroyWindow(window);
	glfwTerminate();
}

static bool g_shouldClose_SDL = false;
bool shouldClose(SDL_Window* window)
{
	return g_shouldClose_SDL;
}

void swapBuffers(SDL_Window* window)
{
	SDL_GL_SwapWindow( window );
}

void destroyWindow(SDL_Window* window)
{
	SDL_DestroyWindow(window);
}

void render(GLFWwindow* window, std::function<void (double)> loop) {
	float lastTime = 0.0;
	while ( !glfwWindowShouldClose(window)) {
		float currentTime =static_cast<float>(glfwGetTime());
		loop(currentTime - lastTime);
		lastTime = currentTime;

		glfwSwapBuffers(window);
		glfwPollEvents();
	}
}

GLenum checkGLError(bool printIfNoError)
{
	GLenum error = glGetError();
	
	switch (error) 
	{
	case GL_INVALID_VALUE:
		DEBUGLOG->log("GL_INVALID_VALUE");
		break;
	case GL_INVALID_ENUM:
		DEBUGLOG->log("GL_INVALID_ENUM");
		break;
	case GL_INVALID_OPERATION:
		DEBUGLOG->log("GL_INVALID_OPERATION");
		break;
	case GL_INVALID_FRAMEBUFFER_OPERATION:
		DEBUGLOG->log("GL_INVALID_FRAMEBUFFER_OPERATION");
		break;
	case GL_OUT_OF_MEMORY:
		DEBUGLOG->log("GL_OUT_OF_MEMORY");
		break;		
	case GL_NO_ERROR:				
		if (printIfNoError)
		{
			DEBUGLOG->log("GL_NO_ERROR"); // do not use error stream for this
		}
		break;
	}
	return error;
}


std::string decodeGLError(GLenum error)
{
	switch (error) 
	{
	case GL_INVALID_VALUE:
		return std::string("GL_INVALID_VALUE");
	case GL_INVALID_ENUM:
		return std::string("GL_INVALID_ENUM");
	case GL_INVALID_OPERATION:
		return std::string("GL_INVALID_OPERATION");
	case GL_INVALID_FRAMEBUFFER_OPERATION:
		return std::string("GL_INVALID_FRAMEBUFFER_OPERATION");
	case GL_OUT_OF_MEMORY:
		return std::string("GL_OUT_OF_MEMORY");
	case GL_NO_ERROR:
		return std::string("GL_NO_ERROR");
	}

	return std::string(); // not a valid error state
}

void pollSDLEvents(SDL_Window* window, std::function<bool(SDL_Event*)> ui_eventHandler)
{

	SDL_Event sdlEvent;
	while (SDL_PollEvent(&sdlEvent) != 0)
	{
		bool uiHandlesEvent = ui_eventHandler(&sdlEvent);
		//TODO do somethings clever with this information?

		switch(sdlEvent.type)
		{
		case SDL_QUIT:
			g_shouldClose_SDL = true;
			break;
		case SDL_KEYDOWN:
		case SDL_KEYUP:
				// TODO handle SDL keyevent
			break;
		case SDL_MOUSEBUTTONDOWN:
		case SDL_MOUSEBUTTONUP:
				// TODO handle SDL mouse button event
			break;
		case SDL_MOUSEMOTION:
				// TODO handle SDL mouse motion event
			break;
		case SDL_MOUSEWHEEL:
				// TODO handle SDL mouse wheel event
			break;
		}
	}
}


void setKeyCallback(GLFWwindow* window, std::function<void (int, int, int, int)> func) {
	static std::function<void (int, int, int, int)> func_bounce = func;
	glfwSetKeyCallback(window, [] (GLFWwindow* w, int k, int s, int a, int m) {
		func_bounce(k, s, a, m);
	});
}

void setMouseButtonCallback(GLFWwindow* window, std::function<void (int, int, int)> func) {
	static std::function<void (int, int, int)> func_bounce = func;
	glfwSetMouseButtonCallback(window, [] (GLFWwindow* w, int b, int a, int m) {
		func_bounce(b, a, m);
	});
}

void setCharCallback(GLFWwindow* window, std::function<void (unsigned int)> func) {
	static std::function<void (unsigned int)> func_bounce = func;
	glfwSetCharCallback(window, [] (GLFWwindow* w, unsigned int c) {
		func_bounce(c);
	});
}

void setCursorPosCallback(GLFWwindow* window, std::function<void (double, double)> func) {
	static std::function<void (double, double)> func_bounce = func;
	glfwSetCursorPosCallback(window, [] (GLFWwindow* w, double x, double y) {
		func_bounce(x, y);
	});
}

void setScrollCallback(GLFWwindow* window, std::function<void (double, double)> func) {
	static std::function<void (double, double)> func_bounce = func;
	glfwSetScrollCallback(window, [] (GLFWwindow* w, double x, double y) {
		func_bounce(x, y);
	});
}

void setCursorEnterCallback(GLFWwindow* window, std::function<void (int)> func) {
	static std::function<void (int)> func_bounce = func;
	glfwSetCursorEnterCallback(window, [] (GLFWwindow* w, int e) {
		func_bounce(e);
	});
}

void setWindowResizeCallback(GLFWwindow* window, std::function<void (int,int)> func) {
	static std::function<void (int,int)> func_bounce = func;
	glfwSetWindowSizeCallback(window, [] (GLFWwindow* w, int wid, int hei) {
		func_bounce(wid, hei);
	});
}


glm::vec2 getResolution(GLFWwindow* window) {
    int w, h;
    glfwGetFramebufferSize(window, &w, &h);
    return glm::vec2(float(w), float(h));
}
float getRatio(GLFWwindow* window) {
    int w, h;
    glfwGetFramebufferSize(window, &w, &h);
    return float(w)/float(h);
}

glm::vec2 getResolution(SDL_Window* window) {
    int w, h;
	SDL_GetWindowSize(window, &w, &h);
    return glm::vec2(float(w), float(h));

}
float getRatio(SDL_Window* window) {
    int w, h;
	SDL_GetWindowSize(window, &w, &h);
    return float(w)/float(h);
}


#include <Rendering/FrameBufferObject.h>
void copyFBOContent(FrameBufferObject* source, FrameBufferObject* target, GLbitfield bitField, GLenum readBuffer, GLenum filter, glm::vec2 defaultFBOSize )
{
	// bind framebuffers
	OPENGLCONTEXT->bindFBO((source != nullptr) ? source->getFramebufferHandle() : 0, GL_READ_FRAMEBUFFER);
	OPENGLCONTEXT->bindFBO((target != nullptr) ? target->getFramebufferHandle() : 0, GL_DRAW_FRAMEBUFFER);
	
	GLint defaultFBOWidth =  (GLint) (defaultFBOSize.x != -1.0f) ? defaultFBOSize.x : g_mainWindowSize.x;
	GLint defaultFBOHeight = (GLint) (defaultFBOSize.y != -1.0f) ? defaultFBOSize.y : g_mainWindowSize.y;
	
	// color buffer is to be copied
	if (bitField == GL_COLOR_BUFFER_BIT)
	{
		// default
		if (readBuffer == GL_NONE)
		{
			glReadBuffer( (source == nullptr) ? GL_BACK : GL_COLOR_ATTACHMENT0);
		}
		else // set manually
		{
			glReadBuffer( readBuffer );
		}
		// copy content

		glBlitFramebuffer(
			0,0,(source != nullptr) ? source->getWidth() : defaultFBOWidth, (source!=nullptr)   ? source->getHeight() : defaultFBOHeight,
			0,0,(target != nullptr) ? target->getWidth() : defaultFBOWidth, (target != nullptr) ? target->getHeight() : defaultFBOHeight,
			bitField, (filter == GL_NONE) ? GL_NEAREST : filter);
		
		OPENGLCONTEXT->bindFBO(0);
		return;
	}
	if (bitField == GL_DEPTH_BUFFER_BIT)
	{
		glBlitFramebuffer(
			0,0,(source != nullptr) ? source->getWidth() : defaultFBOWidth, (source!=nullptr)   ? source->getHeight() : defaultFBOHeight, 
			0,0,(target != nullptr) ? target->getWidth() : defaultFBOWidth, (target != nullptr) ? target->getHeight() : defaultFBOHeight,
			bitField, GL_NEAREST);
		OPENGLCONTEXT->bindFBO(0);
		return;
	}

	// custom
	glBlitFramebuffer(
		0,0,(source != nullptr) ? source->getWidth() : defaultFBOWidth, (source!=nullptr)   ? source->getHeight() : defaultFBOHeight, 
		0,0,(target != nullptr) ? target->getWidth() : defaultFBOWidth, (target != nullptr) ? target->getHeight() : defaultFBOHeight,
		bitField, filter);
	OPENGLCONTEXT->bindFBO(0);
}
void copyFBOContent(GLuint source, GLuint target, glm::vec2 sourceResolution, glm::vec2 targetResolution, GLenum bitField, GLenum readBuffer, GLenum filter)
{
		// bind framebuffers
	OPENGLCONTEXT->bindFBO(source, GL_READ_FRAMEBUFFER);
	OPENGLCONTEXT->bindFBO(target, GL_DRAW_FRAMEBUFFER);
		
	// color buffer is to be copied
	if (bitField == GL_COLOR_BUFFER_BIT)
	{
		// default
		if (readBuffer == GL_NONE)
		{
			glReadBuffer( (source == 0) ? GL_BACK : GL_COLOR_ATTACHMENT0);
		}
		else // set manually
		{
			glReadBuffer( readBuffer );
		}
		// copy content

		glBlitFramebuffer(
			0,0, (GLint) sourceResolution.x, (GLint) sourceResolution.y,
			0,0, (GLint) targetResolution.x, (GLint) targetResolution.y,
			bitField, (filter == GL_NONE) ? GL_NEAREST : filter);
		
		OPENGLCONTEXT->bindFBO(0);
		return;
	}
	if (bitField == GL_DEPTH_BUFFER_BIT)
	{
		glBlitFramebuffer(
			0,0, (GLint) sourceResolution.x, (GLint) sourceResolution.y,
			0,0, (GLint) targetResolution.x, (GLint) targetResolution.y,
			bitField, GL_NEAREST);
		OPENGLCONTEXT->bindFBO(0);
		return;
	}

	// custom
	glBlitFramebuffer(
		0,0, (GLint) sourceResolution.x, (GLint) sourceResolution.y,
		0,0, (GLint) targetResolution.x, (GLint) targetResolution.y,
		bitField, filter);
	OPENGLCONTEXT->bindFBO(0);
}