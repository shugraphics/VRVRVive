/*******************************************
 * **** DESCRIPTION ****
 ****************************************/
#include <iostream>

#include <Rendering/GLTools.h>
#include <Rendering/VertexArrayObjects.h>
#include <Rendering/RenderPass.h>
#include <Volume/ChunkedRenderPass.h>

#include <UI/imgui/imgui.h>
#include <UI/imgui_impl_sdl_gl3.h>
#include <UI/Turntable.h>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/transform.hpp>

#include <Volume/TransferFunction.h>

// openvr includes
#include <openvr.h>
#include <VR/OpenVRTools.h>

////////////////////// PARAMETERS /////////////////////////////
static float s_minValue = INT_MIN; // minimal value in data set; to be overwitten after import
static float s_maxValue = INT_MAX;  // maximal value in data set; to be overwitten after import

static bool  s_isRotating = false; 	// initial state for rotating animation
static float s_rayStepSize = 0.1f;  // ray sampling step size; to be overwritten after volume data import

static float s_rayParamEnd  = 1.0f; // parameter of uvw ray start in volume
static float s_rayParamStart= 0.0f; // parameter of uvw ray end   in volume

static const char* s_models[] = {"CT Head"};

static float s_windowingMinValue = -FLT_MAX / 2.0f;
static float s_windowingMaxValue = FLT_MAX / 2.0f;
static float s_windowingRange = FLT_MAX;

static const float MIRROR_SCREEN_FRAME_INTERVAL = 0.1f; // interval time (seconds) to mirror the screen (to avoid wait for vsync stalls)

static const std::vector<std::string> s_shaderDefines( { 
	"DEPTH_BIAS 0.05", 
	"DEPTH_SCALE 5.0" 
} );

struct TFPoint{
	int v; // value 
	glm::vec4 col; // mapped color
};

static TransferFunction s_transferFunction;

static std::vector<float> s_fpsCounter = std::vector<float>(120);
static int s_curFPSidx = 0;

float s_near = 0.1f;
float s_far = 30.0f;
float s_fovY = 45.0f;
float s_nearH;
float s_nearW;

// matrices
glm::mat4 s_view;
glm::mat4 s_view_r;
glm::mat4 s_perspective;
glm::mat4 s_perspective_r;

glm::mat4 s_screenToView;
glm::mat4 s_modelToTexture;

glm::mat4 s_translation;
glm::mat4 s_rotation;
glm::mat4 s_scale;

glm::vec3 s_volumeSize(1.0f, 0.886f, 1.0);

//////////////////////////////////////////////////////////////////////////////
////////////////////////////// MISC //////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////


void generateTransferFunction()
{
	s_transferFunction.getValues().clear();
	s_transferFunction.getColors().clear();
	s_transferFunction.getValues().push_back(58);
	s_transferFunction.getColors().push_back(glm::vec4(0.0/255.0f, 0.0/255.0f, 0.0/255.0f, 0.0/255.0f));
	s_transferFunction.getValues().push_back(539);
	s_transferFunction.getColors().push_back(glm::vec4(255.0/255.0f, 0.0/255.0f, 0.0/255.0f, 231.0/255.0f));
	s_transferFunction.getValues().push_back(572);
	s_transferFunction.getColors().push_back(glm::vec4(0.0 /255.0f, 74.0 /255.0f, 118.0 /255.0f, 64.0 /255.0f));
	s_transferFunction.getValues().push_back(1356);
	s_transferFunction.getColors().push_back(glm::vec4(0/255.0f, 11.0/255.0f, 112.0/255.0f, 0.0 /255.0f));
	s_transferFunction.getValues().push_back(1500);
	s_transferFunction.getColors().push_back(glm::vec4( 242.0/ 255.0, 212.0/ 255.0, 255.0/ 255.0, 255.0 /255.0f));
}

void updateTransferFunctionTex()
{
	s_transferFunction.updateTex(s_minValue, s_maxValue);
}

template <class T>
void activateVolume(VolumeData<T>& volumeData ) // set static variables
{
	DEBUGLOG->log("File Info:");
	DEBUGLOG->indent();
		DEBUGLOG->log("min value: ", volumeData.min);
		DEBUGLOG->log("max value: ", volumeData.max);
		DEBUGLOG->log("res. x   : ", volumeData.size_x);
		DEBUGLOG->log("res. y   : ", volumeData.size_y);
		DEBUGLOG->log("res. z   : ", volumeData.size_z);
	DEBUGLOG->outdent();

	// set volume specific parameters
	s_minValue = volumeData.min;
	s_maxValue = volumeData.max;
	s_rayStepSize = 1.0f / (2.0f * volumeData.size_x); // this seems a reasonable size
	s_windowingMinValue = (float) volumeData.min;
	s_windowingMaxValue = (float) volumeData.max;
	s_windowingRange = s_windowingMaxValue - s_windowingMinValue;
}

void profileFPS(float fps)
{
	s_fpsCounter[s_curFPSidx] = fps;
	s_curFPSidx = (s_curFPSidx + 1) % s_fpsCounter.size(); 
}

//////////////////////////////////////////////////////////////////////////////
///////////////////////////////// MAIN ///////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////

int main(int argc, char *argv[])
{
	DEBUGLOG->setAutoPrint(true);

	// create window and opengl context
	auto window = generateWindow_SDL(1600,800);
	SDL_DisplayMode currentDisplayMode;
	SDL_GetWindowDisplayMode(window, &currentDisplayMode);

	DEBUGLOG->log("SDL Display Mode");
	{ DEBUGLOG->indent();
	DEBUGLOG->log("w: ", currentDisplayMode.w);
	DEBUGLOG->log("h: ", currentDisplayMode.h);
	DEBUGLOG->log("refresh rate: ", currentDisplayMode.refresh_rate);
	} DEBUGLOG ->outdent();

	// set refresh rate higher
	currentDisplayMode.refresh_rate = 90;
	SDL_SetWindowDisplayMode(window, &currentDisplayMode);

	// check
	SDL_GetWindowDisplayMode(window, &currentDisplayMode);
	DEBUGLOG->log("updated refresh rate: ", currentDisplayMode.refresh_rate);

	//////////////////////////////////////////////////////////////////////////////
	/////////////////////// INITIALIZE OPENVR   //////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////
	
	OpenVRSystem ovr(s_near, s_far);
	
	if ( ovr.initialize() )
	{
		DEBUGLOG->log("Alright! OpenVR up and running!");
		ovr.initializeHMDMatrices();

		ovr.CreateShaders();
		ovr.SetupRenderModels();

		s_fovY = ovr.getFovY();
	}

	//////////////////////////////////////////////////////////////////////////////
	/////////////////////// VOLUME DATA LOADING //////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////

	// load data set: CT of a Head	// load into 3d texture
	std::string file = RESOURCES_PATH;
	file += std::string( "/volumes/CTHead/CThead");
	VolumeData<float> volumeData = Importer::load3DData<float>(file, 256, 256, 113, 2);
	GLuint volumeTextureCT = loadTo3DTexture<float>(volumeData, 8, GL_R16F, GL_RED, GL_FLOAT);

	DEBUGLOG->log("Initial ray sampling step size: ", s_rayStepSize);
	DEBUGLOG->log("Loading Volume Data to 3D-Texture.");

	activateVolume<float>(volumeData);

	//////////////////////////////////////////////////////////////////////////////
	/////////////////////////////// RENDERING  ///////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////
	
	/////////////////////     Scene / View Settings     //////////////////////////
	if (ovr.m_pHMD)
	{
		s_translation = glm::translate(glm::vec3(0.0f,1.0f,-0.5f));
		s_scale = glm::scale(glm::vec3(0.3f,0.3f,0.3f));
	}
	else
	{	
		s_translation = glm::translate(glm::vec3(0.0f,0.0f,-3.0f));
		s_scale = glm::scale(glm::vec3(1.0f,1.0f,1.0f));
	}
	s_rotation = glm::rotate(glm::radians(180.0f), glm::vec3(0.0f,0.0f,1.0f));


	//glm::mat4 model(1.0f)
	glm::vec4 eye(0.0f, 0.0f, 3.0f, 1.0f);
	glm::vec4 center(0.0f,0.0f,0.0f,1.0f);

	// use waitgetPoses to update matrices
	s_view = glm::lookAt(glm::vec3(eye), glm::vec3(center), glm::vec3(0.0f, 1.0f, 0.0f));
	s_view_r = glm::lookAt(glm::vec3(eye) +  glm::vec3(0.15,0.0,0.0), glm::vec3(center), glm::vec3(0.0f, 1.0f, 0.0f));
	if (!ovr.m_pHMD)
	{
		s_perspective = glm::perspective(glm::radians(45.f), getResolution(window).x / (2.0f * getResolution(window).y), s_near, 10.f);
		s_perspective_r = glm::perspective(glm::radians(45.f), getResolution(window).x / (2.0f * getResolution(window).y), s_near, 10.f);
	}
	else
	{
		s_view = ovr.m_mat4eyePosLeft * s_view;
		s_view_r = ovr.m_mat4eyePosRight * s_view_r;
		s_perspective = ovr.m_mat4ProjectionLeft; 
		s_perspective_r = ovr.m_mat4ProjectionRight;
	}
	
	s_nearH = s_near * std::tanf( glm::radians(s_fovY/2.0f) );
	s_nearW = s_nearH * getResolution(window).x / (2.0f * getResolution(window).y);

	// constant
	s_screenToView = glm::scale(glm::vec3(s_nearW, s_nearH, s_near)) * 
		glm::inverse( 
			glm::translate(glm::vec3(0.5f, 0.5f, 0.5f)) * 
			glm::scale(glm::vec3(0.5f,0.5f,0.5f)) 
			);
    
	// constant
	s_modelToTexture = glm::mat4( // swap components
		glm::vec4(1.0f, 0.0f, 0.0f, 0.0f), // column 1
		glm::vec4(0.0f, 0.0f, 1.0f, 0.0f), // column 2
		glm::vec4(0.0f, -1.0f, 0.0f, 0.0f),//column 3
		glm::vec4(0.0f, 0.0f, 0.0f, 1.0f)) //column 4 
		* glm:: inverse(glm::scale( 2.0f * s_volumeSize) ) // moves origin to front left
		* glm::translate( glm::vec3(s_volumeSize.x, s_volumeSize.y, -s_volumeSize.z) );

	// create Volume and VertexGrid
	VolumeSubdiv volume(s_volumeSize.x, s_volumeSize.y, s_volumeSize.z, 3);
	Quad quad;

	///////////////////////     UVW Map Renderpass     ///////////////////////////
	DEBUGLOG->log("Shader Compilation: volume uvw coords"); DEBUGLOG->indent();
	ShaderProgram uvwShaderProgram("/modelSpace/volumeMVP.vert", "/modelSpace/volumeUVW.frag", s_shaderDefines); DEBUGLOG->outdent();
	uvwShaderProgram.update("model", s_translation * s_rotation * s_scale);
	uvwShaderProgram.update("view", s_view);
	uvwShaderProgram.update("projection", s_perspective);

	DEBUGLOG->log("FrameBufferObject Creation: volume uvw coords"); DEBUGLOG->indent();
	FrameBufferObject uvwFBO(getResolution(window).x/2, getResolution(window).y);
	uvwFBO.addColorAttachments(2); // front UVRs and back UVRs
	FrameBufferObject uvwFBO_r(getResolution(window).x/2, getResolution(window).y);
	uvwFBO_r.addColorAttachments(2); // front UVRs and back UVRs
	DEBUGLOG->outdent();
	
	RenderPass uvwRenderPass(&uvwShaderProgram, &uvwFBO);
	uvwRenderPass.addClearBit(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);
	uvwRenderPass.addDisable(GL_DEPTH_TEST); // to prevent back fragments from being discarded
	uvwRenderPass.addEnable(GL_BLEND); // to prevent vec4(0.0) outputs from overwriting previous results
	uvwRenderPass.addRenderable(&volume);

	///////////////////////   Ray-Casting Renderpass    //////////////////////////
	DEBUGLOG->log("Shader Compilation: ray casting shader"); DEBUGLOG->indent();
	ShaderProgram shaderProgram("/raycast/simpleRaycastChunked.vert", "/raycast/simpleRaycastLodDepthOcclusion.frag", s_shaderDefines); DEBUGLOG->outdent();
	shaderProgram.update("uStepSize", s_rayStepSize);
	shaderProgram.update("uViewport", glm::vec4(0,0,getResolution(window).x/2, getResolution(window).y));	
	shaderProgram.update("uResolution", glm::vec4(getResolution(window).x/2, getResolution(window).y,0,0));
	shaderProgram.update("uScreenToView", s_screenToView);

	// DEBUG
	generateTransferFunction();
	updateTransferFunctionTex();

	DEBUGLOG->log("FrameBufferObject Creation: ray casting"); DEBUGLOG->indent();
	FrameBufferObject FBO(shaderProgram.getOutputInfoMap(), getResolution(window).x/2, getResolution(window).y);
	FrameBufferObject FBO_r(shaderProgram.getOutputInfoMap(), getResolution(window).x/2, getResolution(window).y);
	DEBUGLOG->outdent();

	// bind volume texture, back uvw textures, front uvws
	OPENGLCONTEXT->bindTextureToUnit(volumeTextureCT, GL_TEXTURE0, GL_TEXTURE_3D);
	OPENGLCONTEXT->bindTextureToUnit(s_transferFunction.getTextureHandle()						   , GL_TEXTURE3, GL_TEXTURE_1D); // transfer function

	OPENGLCONTEXT->bindTextureToUnit(uvwFBO.getColorAttachmentTextureHandle(  GL_COLOR_ATTACHMENT0), GL_TEXTURE1, GL_TEXTURE_2D); // left uvw back
	OPENGLCONTEXT->bindTextureToUnit(uvwFBO.getColorAttachmentTextureHandle(  GL_COLOR_ATTACHMENT1), GL_TEXTURE2, GL_TEXTURE_2D); // left uvw front

	OPENGLCONTEXT->bindTextureToUnit(uvwFBO_r.getColorAttachmentTextureHandle(GL_COLOR_ATTACHMENT0), GL_TEXTURE4, GL_TEXTURE_2D); // right uvw back
	OPENGLCONTEXT->bindTextureToUnit(uvwFBO_r.getColorAttachmentTextureHandle(GL_COLOR_ATTACHMENT1), GL_TEXTURE5, GL_TEXTURE_2D); // right uvw front

	OPENGLCONTEXT->bindTextureToUnit(FBO.getColorAttachmentTextureHandle(	  GL_COLOR_ATTACHMENT1), GL_TEXTURE6, GL_TEXTURE_2D); // left first hit map
	OPENGLCONTEXT->bindTextureToUnit(FBO_r.getColorAttachmentTextureHandle(	  GL_COLOR_ATTACHMENT1), GL_TEXTURE7, GL_TEXTURE_2D); // right first hit map

	OPENGLCONTEXT->bindTextureToUnit(FBO.getColorAttachmentTextureHandle(GL_COLOR_ATTACHMENT0), GL_TEXTURE10, GL_TEXTURE_2D); // left  raycasting result
	OPENGLCONTEXT->bindTextureToUnit(FBO_r.getColorAttachmentTextureHandle(GL_COLOR_ATTACHMENT0), GL_TEXTURE11, GL_TEXTURE_2D);// right raycasting result

	shaderProgram.update("volume_texture", 0); // volume texture
	shaderProgram.update("transferFunctionTex", 3);

	RenderPass renderPass(&shaderProgram, &FBO);
	renderPass.addClearBit(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );
	renderPass.setClearColor(0.1f,0.12f,0.15f,0.0f);
	renderPass.addRenderable(&quad);
	renderPass.addEnable(GL_DEPTH_TEST);
	renderPass.addDisable(GL_BLEND);

	RenderPass renderPass_r(&shaderProgram, &FBO_r);
	renderPass_r.addClearBit(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );
	renderPass_r.setClearColor(0.1f,0.12f,0.15f,0.0f);
	renderPass_r.addRenderable(&quad);
	renderPass_r.addEnable(GL_DEPTH_TEST);
	renderPass_r.addDisable(GL_BLEND);
	
	///////////////////////   Chunked RenderPasses    //////////////////////////
	glm::ivec2 viewportSize = glm::ivec2(getResolution(window).x / 2, getResolution(window).y);
	glm::ivec2 chunkSize = glm::ivec2(96,96);
	ChunkedAdaptiveRenderPass chunkedRenderPass(
		&renderPass,
		viewportSize,
		chunkSize,
		8
		);
	ChunkedAdaptiveRenderPass chunkedRenderPass_r(
		&renderPass_r,
		viewportSize,
		chunkSize,
		8
		);

	//DEBUG
	chunkedRenderPass.activateClearbits();
	chunkedRenderPass_r.activateClearbits();

	///////////////////////   Occlusion Frustum Renderpass    //////////////////////////
	int occlusionBlockSize = 6;
	int vertexGridWidth  = (int) getResolution(window).x/2 / occlusionBlockSize;
	int vertexGridHeight = (int) getResolution(window).y   / occlusionBlockSize;
	VertexGrid vertexGrid(vertexGridWidth, vertexGridHeight, false, VertexGrid::TOP_RIGHT_COLUMNWISE, glm::ivec2(96, 96)); //dunno what is a good group size?
	ShaderProgram occlusionFrustumShader("/raycast/occlusionFrustum.vert", "/raycast/occlusionFrustum.frag", "/raycast/occlusionFrustum.geom", s_shaderDefines);
	FrameBufferObject occlusionFrustumFBO(   occlusionFrustumShader.getOutputInfoMap(), uvwFBO.getWidth(),   uvwFBO.getHeight() );
	FrameBufferObject occlusionFrustumFBO_r( occlusionFrustumShader.getOutputInfoMap(), uvwFBO_r.getWidth(), uvwFBO_r.getHeight() );
	RenderPass occlusionFrustum(&occlusionFrustumShader, &occlusionFrustumFBO);
	occlusionFrustum.addRenderable(&vertexGrid);
	occlusionFrustum.addClearBit(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	occlusionFrustum.addEnable(GL_DEPTH_TEST);
	occlusionFrustum.addDisable(GL_BLEND);
	occlusionFrustumShader.update("uOcclusionBlockSize", occlusionBlockSize);
	occlusionFrustumShader.update("uGridSize", glm::vec4(vertexGridWidth, vertexGridHeight, 1.0f / (float) vertexGridWidth, 1.0f / vertexGridHeight));
	occlusionFrustumShader.update("uScreenToView", s_screenToView);

	OPENGLCONTEXT->bindTextureToUnit(occlusionFrustumFBO.getColorAttachmentTextureHandle(	  GL_COLOR_ATTACHMENT0), GL_TEXTURE8, GL_TEXTURE_2D); // left occlusion map
	OPENGLCONTEXT->bindTextureToUnit(occlusionFrustumFBO_r.getColorAttachmentTextureHandle(	  GL_COLOR_ATTACHMENT0), GL_TEXTURE9, GL_TEXTURE_2D); // right occlusion map

	OPENGLCONTEXT->bindTextureToUnit(occlusionFrustumFBO.getColorAttachmentTextureHandle(	  GL_COLOR_ATTACHMENT1), GL_TEXTURE12, GL_TEXTURE_2D); // left occlusion map depth
	OPENGLCONTEXT->bindTextureToUnit(occlusionFrustumFBO_r.getColorAttachmentTextureHandle(	  GL_COLOR_ATTACHMENT1), GL_TEXTURE13, GL_TEXTURE_2D); // right occlusion map depth
	///////////////////////   Show Texture Renderpass    //////////////////////////
	ShaderProgram showTexShader("/screenSpace/fullscreen.vert", "/screenSpace/simpleAlphaTexture.frag");
	RenderPass showTex(&showTexShader,0);
	showTex.addRenderable(&quad);

	//////////////////////////////////////////////////////////////////////////////
	///////////////////////    GUI / USER INPUT   ////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////

	// Setup ImGui binding
	ImGui_ImplSdlGL3_Init(window);
    bool show_test_window = true;

	Turntable turntable;
	double old_x;
    double old_y;

	//handles all the sdl events
	auto sdlEventHandler = [&](SDL_Event *event)
	{
		bool imguiHandlesEvent = ImGui_ImplSdlGL3_ProcessEvent(event);
		switch(event->type)
		{
			case SDL_KEYDOWN:
			{
				int k = event->key.keysym.sym;
				switch (k)
				{
					case SDLK_w:
						s_translation = glm::translate( glm::vec3(glm::inverse(s_view) * glm::vec4(0.0f,0.0f,-0.1f,0.0f))) * s_translation;
						break;
					case SDLK_a:
						s_translation = glm::translate(glm::vec3(glm::inverse(s_view) * glm::vec4(-0.1f,0.0f,0.0f,0.0f))) * s_translation;
						break;
					case SDLK_s:
						s_translation = glm::translate(glm::vec3(glm::inverse(s_view) * glm::vec4(0.0f,0.0f,0.1f,0.0f))) * s_translation;
						break;
					case SDLK_d:
						s_translation = glm::translate(glm::vec3(glm::inverse(s_view) * glm::vec4(0.1f,0.0f,0.0f,0.0f))) * s_translation;
						break;
					default:
						break;
				}
				break;
			}
			case SDL_MOUSEMOTION:
			{
				ImGuiIO& io = ImGui::GetIO();
				if ( io.WantCaptureMouse )
				{ break; } // ImGUI is handling this

				double d_x = event->motion.x - old_x;
				double d_y = event->motion.y - old_y;

				if ( turntable.getDragActive() )
				{
					turntable.dragBy(d_x, d_y, s_view);
				}

				old_x = event->motion.x;
				old_y = event->motion.y;
				break;
			}
			case SDL_MOUSEBUTTONDOWN:
			{
				if (event->button.button == SDL_BUTTON_LEFT)
				{
					turntable.setDragActive(true);
				}
				if (event->button.button == SDL_BUTTON_RIGHT)
				{
					unsigned char pick_col[15];
					glReadPixels(old_x-2, getResolution(window).y-old_y, 5, 1, GL_RGB, GL_UNSIGNED_BYTE, pick_col);

					for (int i = 0; i < 15; i += 3)
					{
						DEBUGLOG->log("color: ", glm::vec3(pick_col[i + 0], pick_col[i + 1], pick_col[i+2]));
					}
				}
				break;
			}
			case SDL_MOUSEBUTTONUP:
			{
				if (event->button.button == SDL_BUTTON_LEFT)
				{
					turntable.setDragActive(false);
				}
				break;
			}
		}
		return true;
	};

	std::string window_header = "Volume Renderer - OpenVR";
	SDL_SetWindowTitle(window, window_header.c_str() );

	//////////////////////////////////////////////////////////////////////////////
	//////////////////////////////// RENDER LOOP /////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////

	double elapsedTime = 0.0;
	float mirrorScreenTimer = 0.0f;
	while (!shouldClose(window))
	{
		////////////////////////////////    EVENTS    ////////////////////////////////
		pollSDLEvents(window, sdlEventHandler);
		ovr.PollVREvents();

		////////////////////////////////     GUI      ////////////////////////////////
        ImGuiIO& io = ImGui::GetIO();
		profileFPS(ImGui::GetIO().Framerate);
		ImGui_ImplSdlGL3_NewFrame(window);
	
		ImGui::Value("FPS", io.Framerate);
		mirrorScreenTimer += io.DeltaTime;

		ImGui::PushStyleColor(ImGuiCol_PlotLines, ImVec4(0.2,0.8,0.2,1.0) );
		ImGui::PlotLines("FPS", &s_fpsCounter[0], s_fpsCounter.size(), 0, NULL, 0.0, 65.0, ImVec2(120,60));
		ImGui::PopStyleColor();
	
		ImGui::Columns(2, "mycolumns2", true);
        ImGui::Separator();
		bool changed = false;
		for (unsigned int n = 0; n < s_transferFunction.getValues().size(); n++)
        {
			changed |= ImGui::DragInt(("V" + std::to_string(n)).c_str(), &s_transferFunction.getValues()[n], 1.0, s_minValue, s_maxValue);
			ImGui::NextColumn();
			changed |= ImGui::ColorEdit4(("C" + std::to_string(n)).c_str(), &s_transferFunction.getColors()[n][0]);
            ImGui::NextColumn();
        }

		if(changed)
		{
			updateTransferFunctionTex();
		}
        ImGui::Columns(1);
        ImGui::Separator();

		ImGui::PushItemWidth(-100);
		if (ImGui::CollapsingHeader("Volume Rendering Settings"))
    	{
            ImGui::Text("Parameters related to volume rendering");
            ImGui::DragFloatRange2("windowing range", &s_windowingMinValue, &s_windowingMaxValue, 5.0f, (float) s_minValue, (float) s_maxValue); // grayscale ramp boundaries
        	ImGui::SliderFloat("ray step size",   &s_rayStepSize,  0.0001f, 0.1f, "%.5f", 2.0f);
        }
        
		ImGui::Checkbox("auto-rotate", &s_isRotating); // enable/disable rotating volume
		ImGui::PopItemWidth();

		static float lodScale = 2.0f;
		static float lodBias  = 0.25f;
		ImGui::DragFloat("Lod Scale", &lodScale, 0.1f,0.0f,20.0f);
		ImGui::DragFloat("Lod Bias", &lodBias, 0.01f,0.0f,1.2f);

		static bool profiler_visible = false;
		ImGui::Checkbox("Chunk Performance Profiler", &profiler_visible);
		if (profiler_visible) { chunkedRenderPass.imguiInterface(&profiler_visible); };

		static int  leftDebugView = 10;
		static int rightDebugView = 11;
		ImGui::Columns(2);
		ImGui::SliderInt("Left Debug View", &leftDebugView, 1, 13);
		ImGui::NextColumn();
		ImGui::SliderInt("Right Debug View", &rightDebugView, 1, 13);
		ImGui::NextColumn();
		ImGui::Columns(1);
		

        //////////////////////////////////////////////////////////////////////////////

		///////////////////////////// MATRIX UPDATING ///////////////////////////////
		if (s_isRotating) // update s_view matrix
		{
			s_rotation = glm::rotate(glm::mat4(1.0f), (float) io.DeltaTime, glm::vec3(0.0f, 1.0f, 0.0f) ) * s_rotation;
		}

		// use waitgetPoses to update matrices, or just use regular stuff
		if (!ovr.m_pHMD)
		{
			s_view = glm::lookAt(glm::vec3(eye), glm::vec3(center), glm::vec3(0.0f, 1.0f, 0.0f));
			s_view_r = glm::lookAt(glm::vec3(eye) +  glm::vec3(0.15,0.0,0.0), glm::vec3(center), glm::vec3(0.0f, 1.0f, 0.0f));
		}
		else
		{
			ovr.updateTrackedDevicePoses();
			s_view = ovr.m_mat4eyePosLeft * ovr.m_mat4HMDPose;
			s_view_r = ovr.m_mat4eyePosRight * ovr.m_mat4HMDPose;
		}

		// Update Matrices
		// compute current auxiliary matrices
		glm::mat4 invView = glm::inverse(s_view);
		glm::mat4 invView_r = glm::inverse(s_view_r);
		
		glm::mat4 model = s_translation * turntable.getRotationMatrix() * s_rotation * s_scale;
		glm::mat4 invModel = glm::inverse( model ); 
	
		glm::mat4 screenToTexture   = s_modelToTexture * invModel * invView   * s_screenToView;
		glm::mat4 screenToTexture_r = s_modelToTexture * invModel * invView_r * s_screenToView;

		//++++++++++++++ DEBUG			
		static glm::mat4 invOldModel = invModel;   // state of model matrix when left was rendered
		static glm::mat4 invOldModel_r = invModel; // state of model matrix when right was rendered
		static glm::mat4 invOldView   = glm::inverse(s_view);
		static glm::mat4 invOldView_r = glm::inverse(s_view_r);
		static glm::mat4 oldScreenToTexture = screenToTexture;
		static glm::mat4 oldScreenToTexture_r = screenToTexture_r;
		static glm::mat4 oldViewToTexture   = s_modelToTexture * invOldModel   * invOldView;
		static glm::mat4 oldViewToTexture_r = s_modelToTexture * invOldModel_r * invOldView_r;
		//++++++++++++++ DEBUG

		//////////////////////////////////////////////////////////////////////////////
				
		////////////////////////  SHADER / UNIFORM UPDATING //////////////////////////
		// update view related uniforms
		uvwShaderProgram.update("model", model);

		/************* update color mapping parameters ******************/
		// ray start/end parameters
		shaderProgram.update("uStepSize", s_rayStepSize); 	  // ray step size
		shaderProgram.update("uLodBias", lodBias);
		shaderProgram.update("uLodDepthScale", lodScale);  

		// color mapping parameters
		shaderProgram.update("uWindowingMinVal", s_windowingMinValue); 	  // lower grayscale ramp boundary
		shaderProgram.update("uWindowingRange",  s_windowingMaxValue - s_windowingMinValue); // full range of values in window
		//////////////////////////////////////////////////////////////////////////////
		
		////////////////////////////////  RENDERING //// /////////////////////////////
		glDisable(GL_BLEND);
		glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA); // this is altered by ImGui::Render(), so set it every frame
		
		//%%%%%%%%%%%% render left image
		if (chunkedRenderPass.isFinished())
		{
			//update raycasting matrices for next iteration	// for occlusion frustum
			glm::mat4 oldViewToNewView  = s_view * (model * invOldModel) * invOldView;   // old view to new view space
			
			// uvw maps
			uvwRenderPass.setFrameBufferObject( &uvwFBO );
			uvwShaderProgram.update("view", s_view);
			uvwShaderProgram.update("projection", s_perspective);
			uvwRenderPass.render();

			// occlusion maps 
			occlusionFrustum.setFrameBufferObject( &occlusionFrustumFBO );
			occlusionFrustumShader.update("first_hit_map", 6); // left first hit map
			occlusionFrustumShader.update("uViewToNewView", oldViewToNewView);
			occlusionFrustumShader.update("uViewToTexture", oldViewToTexture);
			occlusionFrustumShader.update("uProjection", s_perspective);
			occlusionFrustum.render();

			invOldView = invView; // save current view
			invOldModel = invModel;// save current model 
			oldScreenToTexture = screenToTexture; // save current screenToTexture matrix
			oldViewToTexture = s_modelToTexture * invOldModel * invOldView;
		}

		// raycasting (chunked)
		shaderProgram.update("uScreenToTexture", oldScreenToTexture);
		shaderProgram.update("uViewToTexture", oldViewToTexture);
		shaderProgram.update("back_uvw_map",  1);
		shaderProgram.update("front_uvw_map", 2);
		shaderProgram.update("occlusion_map", 8);

		//if (ImGui::Button("Render Left"))
		{
			//invOldModel = invModel;// save current model 
			//invOldView = invView;
			renderPass.render(); // use chunked rendering instead
		}//chunkedRenderPass.render(); 
		
		//+++++++++ DEBUG  +++++++++++++++++++++++++++++++++++++++++++ 
		//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
		
		//%%%%%%%%%%%% render right image
		// uvw maps
		if (chunkedRenderPass_r.isFinished())
		{
			glm::mat4 oldViewToNewView_r = s_view_r * (model * invOldModel_r) *invOldView_r; // old view to new projection space

			uvwRenderPass.setFrameBufferObject( &uvwFBO_r );
			uvwShaderProgram.update("view", s_view_r);
			uvwShaderProgram.update("projection", s_perspective_r);
			uvwRenderPass.render();
		
			// occlusion maps 
			occlusionFrustum.setFrameBufferObject( &occlusionFrustumFBO_r );
			occlusionFrustumShader.update("first_hit_map", 7); // right first hit map
			occlusionFrustumShader.update("uProjection", s_perspective_r);
			occlusionFrustumShader.update("uViewToNewView", oldViewToNewView_r);
			occlusionFrustumShader.update("uViewToTexture", oldViewToTexture_r);
			occlusionFrustum.render();

			invOldView_r = invView_r; // save current view
			invOldModel_r = invModel;// save current model 
			oldScreenToTexture_r = screenToTexture_r; // save current screenToTexture matrix
			oldViewToTexture_r = s_modelToTexture * invOldModel_r * invOldView_r;
		}

		// raycasting (chunked)
		shaderProgram.update("uScreenToTexture", oldScreenToTexture_r);
		shaderProgram.update("uViewToTexture", oldViewToTexture_r);
		shaderProgram.update("back_uvw_map",  4);
		shaderProgram.update("front_uvw_map", 5);
		shaderProgram.update("occlusion_map", 9);

		renderPass_r.render();
		//chunkedRenderPass_r.render();

		if ( ovr.m_pHMD ) // submit images only when finished
		{
			if ( chunkedRenderPass.isFinished() )
			{
				FBO.bind();
				OPENGLCONTEXT->setEnabled(GL_DEPTH_TEST, true);
				ovr.renderModels(vr::Eye_Left);
				ovr.submitImage(FBO.getColorAttachmentTextureHandle(GL_COLOR_ATTACHMENT0), vr::Eye_Left);
			}

			if ( chunkedRenderPass_r.isFinished() )
			{
				FBO_r.bind();
				OPENGLCONTEXT->setEnabled(GL_DEPTH_TEST, true);
				ovr.renderModels(vr::Eye_Right);
				ovr.submitImage(FBO_r.getColorAttachmentTextureHandle(GL_COLOR_ATTACHMENT0), vr::Eye_Right);
			}
			OPENGLCONTEXT->setEnabled(GL_DEPTH_TEST, false);
		}

		if (mirrorScreenTimer > MIRROR_SCREEN_FRAME_INTERVAL || !ovr.m_pHMD)
		{
			//if (chunkedRenderPass.isFinished())
			{
				showTexShader.update("tex", leftDebugView);
				showTex.setViewport(0,0,(int) getResolution(window).x/2, (int) getResolution(window).y);
				showTex.render();
			}
			//if (chunkedRenderPass_r.isFinished())
			{
				showTexShader.update("tex", rightDebugView);
				showTex.setViewport((int) getResolution(window).x/2,0,(int) getResolution(window).x/2, (int) getResolution(window).y);
				showTex.render();
			}
			//////////////////////////////////////////////////////////////////////////////
			ImGui::Render();
			SDL_GL_SwapWindow( window ); // swap buffers

			mirrorScreenTimer = 0.0f;
		}
		else
		{
			glFinish(); // just Flush
		}
	}
	
	ImGui_ImplSdlGL3_Shutdown();
	ovr.shutdown();
	destroyWindow(window);
	SDL_Quit();

	return 0;
}