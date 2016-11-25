/*******************************************
 * **** DESCRIPTION ****
 ****************************************/
#include <iostream>

#include <Rendering/GLTools.h>
#include <Rendering/VertexArrayObjects.h>
#include <Rendering/RenderPass.h>

#include "UI/imgui/imgui.h"
#include "UI/imgui/imgui_color_picker.cpp"
#include <UI/imguiTools.h>
#include <UI/Turntable.h>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/transform.hpp>

////////////////////// PARAMETERS /////////////////////////////
static float s_minValue = INT_MIN; // minimal value in data set; to be overwitten after import
static float s_maxValue = INT_MAX;  // maximal value in data set; to be overwitten after import

static bool  s_isRotating = false; 	// initial state for rotating animation
static float s_rayStepSize = 0.1f;  // ray sampling step size; to be overwritten after volume data import

static float s_rayParamEnd  = 1.0f; // parameter of uvw ray start in volume
static float s_rayParamStart= 0.0f; // parameter of uvw ray end   in volume

static float s_eyeDistance = 0.15f;

static const char* s_models[] = {"CT Head"};

static float s_windowingMinValue = -FLT_MAX / 2.0f;
static float s_windowingMaxValue = FLT_MAX / 2.0f;
static float s_windowingRange = FLT_MAX;

struct TFPoint{
	int v; // value 
	glm::vec4 col; // mapped color
};

static std::vector<float> s_transferFunctionValues;
static std::vector<glm::vec4> s_transferFunctionColors;
static std::vector<float> s_transferFunctionTexData = std::vector<float>(512*4);
GLuint s_transferFunctionTex = -1;

static std::vector<float> s_fpsCounter = std::vector<float>(120);
static int s_curFPSidx = 0;

//////////////////////////////////////////////////////////////////////////////
///////////////////////////////// MAIN ///////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////

void generateTransferFunction()
{
	s_transferFunctionValues.clear();
	s_transferFunctionColors.clear();
	s_transferFunctionValues.push_back(164);
	s_transferFunctionColors.push_back(glm::vec4(0.0, 0.0, 0.0, 0.0));
	s_transferFunctionValues.push_back(312);
	s_transferFunctionColors.push_back(glm::vec4(1.0, 0.07, 0.07, 0.6));
	s_transferFunctionValues.push_back(872);
	s_transferFunctionColors.push_back(glm::vec4(0.0, 0.5, 1.0, 0.3));
	s_transferFunctionValues.push_back(1142);
	s_transferFunctionColors.push_back(glm::vec4(0.4, 0.3, 0.8, 0.0));
	s_transferFunctionValues.push_back(2500);
	s_transferFunctionColors.push_back(glm::vec4(0.95, 0.83, 1.0, 1.0));
}

void updateTransferFunctionTex()
{
	int currentMin = s_minValue;
	int currentMax = s_minValue+1;
	glm::vec4 currentMinCol( 0.0f, 0.0f, 0.0f, 0.0f );
	glm::vec4 currentMaxCol( 0.0f, 0.0f, 0.0f, 0.0f );

	int currentPoint = -1;
	for (unsigned int i = 0; i < s_transferFunctionTexData.size() / 4; i++)
	{
		glm::vec4 c( 0.0f, 0.0f, 0.0f, 0.0f );
		float relVal = (float) i / (float) (s_transferFunctionTexData.size() / 4);
		int v = relVal * (s_maxValue - s_minValue) + s_minValue;

		if (currentMax < v)
		{
			currentPoint++;
			if (currentPoint < s_transferFunctionValues.size())
			{
				currentMin = currentMax;
				currentMinCol = currentMaxCol;

				currentMax = (int) s_transferFunctionValues[currentPoint];
				currentMaxCol = s_transferFunctionColors[currentPoint];
			}
			else {
				currentMin = currentMax;
				currentMinCol = currentMaxCol;

				currentMax = s_maxValue;
			}
		}

		float mixParam = (float) (v - currentMin) / (float) (currentMax - currentMin);
		c = (1.0f - mixParam) * currentMinCol  + mixParam * currentMaxCol;

		s_transferFunctionTexData[i * 4 +0] = c[0];
		s_transferFunctionTexData[i * 4 +1] = c[1];
		s_transferFunctionTexData[i * 4 +2] = c[2];
		s_transferFunctionTexData[i * 4 +3] = c[3];
	}

	// Upload to texture
	if (s_transferFunctionTex == -1)
	{
		OPENGLCONTEXT->activeTexture(GL_TEXTURE0);
		glGenTextures(1, &s_transferFunctionTex);
		OPENGLCONTEXT->bindTexture(s_transferFunctionTex, GL_TEXTURE_1D);

		glTexStorage1D(GL_TEXTURE_1D, 1, GL_RGBA8, s_transferFunctionTexData.size() / 4);

		glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);

		OPENGLCONTEXT->bindTexture(0, GL_TEXTURE_1D);
	}

	OPENGLCONTEXT->bindTexture(s_transferFunctionTex, GL_TEXTURE_1D);
	glTexSubImage1D(GL_TEXTURE_1D, 0, 0, s_transferFunctionTexData.size() / 4, GL_RGBA, GL_FLOAT, &s_transferFunctionTexData[0]);
	OPENGLCONTEXT->bindTexture(0);
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

int main(int argc, char *argv[])
{
	DEBUGLOG->setAutoPrint(true);

	//////////////////////////////////////////////////////////////////////////////
	/////////////////////// VOLUME DATA LOADING //////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////

	// create window and opengl context
	auto window = generateWindow(1600,800);

	// load data set: CT of a Head	// load into 3d texture
	std::string file = RESOURCES_PATH + std::string( "/volumes/CTHead/CThead");
	VolumeData<short> volumeDataCTHead = Importer::load3DData<short>(file, 256, 256, 113, 2);
	activateVolume<short>(volumeDataCTHead);
	DEBUGLOG->log("Initial ray sampling step size: ", s_rayStepSize);
	DEBUGLOG->log("Loading Volume Data to 3D-Texture.");
	GLuint volumeTextureCT = loadTo3DTexture<short>(volumeDataCTHead);

	//////////////////////////////////////////////////////////////////////////////
	/////////////////////////////// RENDERING  ///////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////
	
	/////////////////////     Scene / View Settings     //////////////////////////
	glm::mat4 model = glm::mat4(1.0f);
	model[1] = glm::vec4(0.0f, -1.0f, 0.0f, 0.0f); // flip y, based on data set
	glm::vec4 eye(2.5f, 0.5f, 2.5f, 1.0f);
	glm::vec4 center(0.0f,0.0f,0.0f,1.0f);
	glm::mat4 view   = glm::lookAt(glm::vec3(eye) - glm::vec3(s_eyeDistance/2.0f,0.0f,0.0f), glm::vec3(center), glm::vec3(0,1,0));
	glm::mat4 view_r = glm::lookAt(glm::vec3(eye) +  glm::vec3(s_eyeDistance/2.0f,0.0f,0.0f), glm::vec3(center) +  glm::vec3(0.0f,0.0f,0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
	glm::mat4 perspective = glm::perspective(glm::radians(45.f), getRatio(window), 1.0f, 10.f);
	glm::mat4 viewprojection_r = perspective * view_r;

	// create Volume and VertexGrid
	VolumeSubdiv volume(1.0f, 0.886f, 1.0f, 3);
	VertexGrid vertexGrid(getResolution(window).x/2, getResolution(window).y);
	Quad quad;

	///////////////////////     UVW Map Renderpass     ///////////////////////////
	DEBUGLOG->log("Shader Compilation: volume uvw coords"); DEBUGLOG->indent();
	ShaderProgram uvwShaderProgram("/modelSpace/volumeMVP.vert", "/modelSpace/volumeUVW.frag"); DEBUGLOG->outdent();
	uvwShaderProgram.update("model", model);
	uvwShaderProgram.update("view", view);
	uvwShaderProgram.update("projection", perspective);

	DEBUGLOG->log("FrameBufferObject Creation: volume uvw coords"); DEBUGLOG->indent();
	FrameBufferObject uvwFBO(getResolution(window).x/2, getResolution(window).y);
	uvwFBO.addColorAttachments(2); // front UVRs and back UVRs

	FrameBufferObject::s_internalFormat = GL_RGBA16F; // allow arbitrary values
	uvwFBO.addColorAttachments(2); // front positions and back positions
	FrameBufferObject::s_internalFormat = GL_RGBA; // default
	DEBUGLOG->outdent(); 

	FrameBufferObject uvwFBO_r(getResolution(window).x/2, getResolution(window).y);
	uvwFBO_r.addColorAttachments(2); DEBUGLOG->outdent(); // front UVRs and back UVRs
	uvwFBO_r.addColorAttachments(2); // front positions and back positions
	
	RenderPass uvwRenderPass(&uvwShaderProgram, &uvwFBO);
	uvwRenderPass.addClearBit(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);
	uvwRenderPass.addDisable(GL_DEPTH_TEST); // to prevent back fragments from being discarded
	uvwRenderPass.addEnable(GL_BLEND); // to prevent vec4(0.0) outputs from overwriting previous results
	uvwRenderPass.addRenderable(&volume);

	///////////////////////   Ray-Casting Renderpass    //////////////////////////
	DEBUGLOG->log("Shader Compilation: ray casting shader"); DEBUGLOG->indent();
	ShaderProgram shaderProgram("/screenSpace/volumeStereo.vert", "/screenSpace/volumeStereo.frag"); DEBUGLOG->outdent();
	shaderProgram.update("projection", perspective);
	shaderProgram.update("uStepSize", s_rayStepSize);
		
	// DEBUG
	generateTransferFunction();
	updateTransferFunctionTex();

	// bind volume texture, back uvw textures, front uvws
	OPENGLCONTEXT->bindTextureToUnit(volumeTextureCT, GL_TEXTURE0, GL_TEXTURE_3D);
	OPENGLCONTEXT->bindTextureToUnit(uvwFBO.getColorAttachmentTextureHandle(GL_COLOR_ATTACHMENT0), GL_TEXTURE1, GL_TEXTURE_2D);
	OPENGLCONTEXT->bindTextureToUnit(uvwFBO.getColorAttachmentTextureHandle(GL_COLOR_ATTACHMENT1), GL_TEXTURE2, GL_TEXTURE_2D);
	OPENGLCONTEXT->bindTextureToUnit(s_transferFunctionTex, GL_TEXTURE3, GL_TEXTURE_1D);
	OPENGLCONTEXT->bindTextureToUnit(uvwFBO.getColorAttachmentTextureHandle(GL_COLOR_ATTACHMENT2), GL_TEXTURE4, GL_TEXTURE_2D);
	OPENGLCONTEXT->bindTextureToUnit(uvwFBO.getColorAttachmentTextureHandle(GL_COLOR_ATTACHMENT3), GL_TEXTURE5, GL_TEXTURE_2D);
	OPENGLCONTEXT->activeTexture(GL_TEXTURE0);

	// generate and bind right view image texture
	GLuint outputTexture = createTexture((int) getResolution(window).x/2, (int)getResolution(window).y, GL_RGBA16F);
	OPENGLCONTEXT->bindImageTextureToUnit(outputTexture,  0, GL_RGBA16F, GL_READ_WRITE);
	OPENGLCONTEXT->bindTextureToUnit(outputTexture, GL_TEXTURE6, GL_TEXTURE_2D); // for display

	// DEBUG
	std::vector<float> texData(((int) getResolution(window).x/2) * (int)getResolution(window).y*4, 0.0f);
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, (int) getResolution(window).x/2, (int)getResolution(window).y, GL_RGBA, GL_FLOAT, &texData[0]);

	shaderProgram.update("volume_texture", 0); // volume texture
	shaderProgram.update("back_uvw_map",  1);
	shaderProgram.update("front_uvw_map", 2);
	shaderProgram.update("transferFunctionTex", 3);
	shaderProgram.update("back_pos_map",  4);
	shaderProgram.update("front_pos_map", 5);

	// ray casting render pass
	RenderPass renderPass(&shaderProgram);
	renderPass.addClearBit(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );
	renderPass.addRenderable(&vertexGrid);
	renderPass.addEnable(GL_DEPTH_TEST);
	renderPass.addDisable(GL_BLEND);

	///////////////////////   Show Texture Renderpass    //////////////////////////
	ShaderProgram showTexShader("/screenSpace/fullscreen.vert", "/screenSpace/simpleAlphaTexture.frag");
	RenderPass showTex(&showTexShader,0);
	showTex.addRenderable(&quad);
	showTex.setViewport((int) getResolution(window).x/2,0,(int) getResolution(window).x/2, (int) getResolution(window).y);
	showTexShader.update( "tex", 6); // output texture

	//////////////////////////////////////////////////////////////////////////////
	///////////////////////    GUI / USER INPUT   ////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////

	// Setup ImGui binding
    ImGui_ImplGlfwGL3_Init(window, true);
    bool show_test_window = true;

	Turntable turntable;
	double old_x;
    double old_y;
	glfwGetCursorPos(window, &old_x, &old_y);
	
	auto cursorPosCB = [&](double x, double y)
	{
		ImGuiIO& io = ImGui::GetIO();
		if ( io.WantCaptureMouse )
		{ return; } // ImGUI is handling this

		double d_x = x - old_x;
		double d_y = y - old_y;

		if ( turntable.getDragActive() )
		{
			turntable.dragBy(d_x, d_y, view);
		}

		old_x = x;
		old_y = y;
	};

	auto mouseButtonCB = [&](int b, int a, int m)
	{
		if (b == GLFW_MOUSE_BUTTON_LEFT && a == GLFW_PRESS)
		{
			turntable.setDragActive(true);
		}
		if (b == GLFW_MOUSE_BUTTON_LEFT && a == GLFW_RELEASE)
		{
			turntable.setDragActive(false);
		}

		ImGui_ImplGlfwGL3_MouseButtonCallback(window, b, a, m);
	};

	auto keyboardCB = [&](int k, int s, int a, int m)
	{
		if (a == GLFW_RELEASE) {return;} 
		switch (k)
		{
			case GLFW_KEY_W:
				eye += glm::inverse(view)    * glm::vec4(0.0f,0.0f,-0.1f,0.0f);
				center += glm::inverse(view) * glm::vec4(0.0f,0.0f,-0.1f,0.0f);
				break;
			case GLFW_KEY_A:
				eye += glm::inverse(view)	 * glm::vec4(-0.1f,0.0f,0.0f,0.0f);
				center += glm::inverse(view) * glm::vec4(-0.1f,0.0f,0.0f,0.0f);
				break;
			case GLFW_KEY_S:
				eye += glm::inverse(view)    * glm::vec4(0.0f,0.0f,0.1f,0.0f);
				center += glm::inverse(view) * glm::vec4(0.0f,0.0f,0.1f,0.0f);
				break;
			case GLFW_KEY_D:
				eye += glm::inverse(view)    * glm::vec4(0.1f,0.0f,0.0f,0.0f);
				center += glm::inverse(view) * glm::vec4(0.1f,0.0f,0.0f,0.0f);
				break;
			default:
				break;
			case GLFW_KEY_R:
			static bool swap = true;
			if (swap){
				renderPass.clearRenderables();
				renderPass.addRenderable(&quad);
				DEBUGLOG->log("renderable: Quad");
				swap = false;
			} else {
				renderPass.clearRenderables();
				renderPass.addRenderable(&vertexGrid);
				DEBUGLOG->log("renderable: Vertex Grid");
				swap = true;
			}
			break;
		}
		ImGui_ImplGlfwGL3_KeyCallback(window,k,s,a,m);
	};

	setCursorPosCallback(window, cursorPosCB);
	setMouseButtonCallback(window, mouseButtonCB);
	setKeyCallback(window, keyboardCB);

	std::string window_header = "Volume Renderer";
	glfwSetWindowTitle(window, window_header.c_str() );

	//////////////////////////////////////////////////////////////////////////////
	//////////////////////////////// RENDER LOOP /////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////

	//++++++++++++++ DEBUG
	glm::mat4 m = 
		(perspective * // -1 .. -1
			view_r * glm::inverse(view) * // from one view to the other
		glm::inverse(perspective) ) * // world with non-one w
		
		glm::translate(glm::vec3(-1.f, -1.f, -1.f)) * //-1..1 
		glm::scale(glm::vec3(2.0f,2.0f,2.0f)) // 0..2
		;
	
	DEBUGLOG->log("m", m);

	glm::vec4 p1(0.0f,0.0f,0.0f,1.0f); // bottom left on near plane
	glm::vec4 p2(0.5f,0.5f,1.0f,1.0f); // center on far plane
	glm::vec4 p3(0.5f,0.5f,0.0f,1.0f); // center on near plane

	DEBUGLOG->log("bottom left: ", m * p1);
	DEBUGLOG->log("center far : ", m * p2);
	DEBUGLOG->log("center near: ", m * p3);

	glm::vec4 p1n =  (m * p1) / (m * p1).w; // bottom left on near plane
	glm::vec4 p2n =  (m * p2) / (m * p2).w; // center on far plane
	glm::vec4 p3n =  (m * p3) / (m * p3).w; // center on near plane

	DEBUGLOG->log("bottom left {normalized}: ", p1n);
	DEBUGLOG->log("center far  {normalized}: ", p2n);
	DEBUGLOG->log("center near {normalized}: ", p3n);

	glm::mat4 bias = glm::translate(glm::vec3(0.5f, 0.5f, 0.5f)) * glm::scale(glm::vec3(0.5f,0.5f,0.5f));
	
	DEBUGLOG->log("bottom left : ", bias * p1n);
	DEBUGLOG->log("center far  : ", bias * p2n);
	DEBUGLOG->log("center near : ", bias * p3n);

	//++++++++++++++ DEBUG


	
	double elapsedTime = 0.0;
	render(window, [&](double dt)
	{
		profileFPS((float) (1.0 / dt));

		////////////////////////////////     GUI      ////////////////////////////////
        ImGuiIO& io = ImGui::GetIO();
		ImGui_ImplGlfwGL3_NewFrame(); // tell ImGui a new frame is being rendered

		ImGui::Value("FPS", (float) (1.0 / dt));

		ImGui::PushStyleColor(ImGuiCol_PlotLines, ImVec4(0.2,0.8,0.2,1.0) );
		ImGui::PlotLines("FPS", &s_fpsCounter[0], s_fpsCounter.size(), 0, NULL, 0.0, 65.0, ImVec2(120,60));
		ImGui::PopStyleColor();

		ImGui::DragFloat("eye distance", &s_eyeDistance, 0.01f, 0.01f, 2.0f); // grayscale ramp boundaries
	
		ImGui::Columns(2, "mycolumns2", true);
        ImGui::Separator();
		bool changed = false;
		for (unsigned int n = 0; n < s_transferFunctionValues.size(); n++)
        {
			changed |= ImGui::DragFloat(("V" + std::to_string(n)).c_str(), &s_transferFunctionValues[n], 1.0f, s_minValue, s_maxValue);
			ImGui::NextColumn();
			changed |= ImGui::ColorEdit4(("C" + std::to_string(n)).c_str(), &s_transferFunctionColors[n][0]);
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
        //////////////////////////////////////////////////////////////////////////////

		///////////////////////////// MATRIX UPDATING ///////////////////////////////
		if (s_isRotating) // update view matrix
		{
			model = glm::rotate(glm::mat4(1.0f), (float) dt, glm::vec3(0.0f, 1.0f, 0.0f) ) * model;
		}

		view = glm::lookAt(glm::vec3(eye) - glm::vec3(s_eyeDistance/2.0,0.0,0.0), glm::vec3(center) - glm::vec3(s_eyeDistance/2.0,0.0,0.0), glm::vec3(0.0f, 1.0f, 0.0f));
		view_r = glm::lookAt(glm::vec3(eye) +  glm::vec3(s_eyeDistance/2.0,0.0f,0.0f), glm::vec3(center) + glm::vec3(s_eyeDistance/2.0,0.0f,0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
		viewprojection_r = perspective * view_r;
		//////////////////////////////////////////////////////////////////////////////
				
		////////////////////////  SHADER / UNIFORM UPDATING //////////////////////////
		// update view related uniforms
		uvwShaderProgram.update("model", turntable.getRotationMatrix() * model);
		uvwShaderProgram.update("view", view);

		shaderProgram.update( "viewprojection_r", viewprojection_r );

		/************* update color mapping parameters ******************/
		// ray start/end parameters
		shaderProgram.update("uStepSize", s_rayStepSize); 	  // ray step size

		// color mapping parameters
		shaderProgram.update("uWindowingMinVal", s_windowingMinValue); 	  // lower grayscale ramp boundary
		shaderProgram.update("uWindowingRange",  s_windowingMaxValue - s_windowingMinValue); // full range of values in window

		/************* update experimental  parameters ******************/
		//////////////////////////////////////////////////////////////////////////////
		
		////////////////////////////////  RENDERING //// /////////////////////////////
		glDisable(GL_BLEND);
		glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA); // this is altered by ImGui::Render(), so set it every frame
		
		// clear output image (very slow!)
		OPENGLCONTEXT->activeTexture(GL_TEXTURE6);
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, (int) getResolution(window).x/2, (int)getResolution(window).y, GL_RGBA, GL_FLOAT, &texData[0]);
		OPENGLCONTEXT->activeTexture(GL_TEXTURE0);

		// render left image
		uvwRenderPass.render();
		
		OPENGLCONTEXT->bindTextureToUnit(uvwFBO.getColorAttachmentTextureHandle(GL_COLOR_ATTACHMENT0), GL_TEXTURE1, GL_TEXTURE_2D);
		OPENGLCONTEXT->bindTextureToUnit(uvwFBO.getColorAttachmentTextureHandle(GL_COLOR_ATTACHMENT1), GL_TEXTURE2, GL_TEXTURE_2D);
		renderPass.setViewport(0, 0, getResolution(window).x / 2, getResolution(window).y);
		renderPass.render();

		// display right image
		showTex.render();

		ImGui::Render();
		//////////////////////////////////////////////////////////////////////////////
	});

	destroyWindow(window);

	return 0;
}