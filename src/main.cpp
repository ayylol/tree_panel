#include <fstream>
#include <imgui.h>
#include <iostream>

// Window.h `#include`s ImGui, GLFW, and glad in correct order.
#include "Window.h"

#include "GLDebug.h"
#include "Geometry.h"
#include "Log.h"
#include "Shader.h"
#include "ShaderProgram.h"
#include <nlohmann/json.hpp>
#include <utility>
using json = nlohmann::json;

void json_to_gui(json &option, std::string name);
std::pair<float,float> get_range_f(std::string name);
std::pair<int, int> get_range_i(std::string name);

// CALLBACKS
class MyCallbacks : public CallbackInterface {

public:
  // Constructor. We use values of -1 for attributes that, at the start of
  // the program, have no meaningful/"true" value.
  MyCallbacks(int screenWidth, int screenHeight)
      : currentFrame(0), leftMouseActiveVal(false),
        lastLeftPressedFrame(-1), lastRightPressedFrame(-1), screenMouseX(-1.0),
        screenMouseY(-1.0), screenWidth(screenWidth),
        screenHeight(screenHeight) {}

  virtual void mouseButtonCallback(int button, int action, int mods) {
    // If we click the mouse on the ImGui window, we don't want to log that
    // here. But if we RELEASE the mouse over the window, we do want to
    // know that!
    auto &io = ImGui::GetIO();
    if (io.WantCaptureMouse && action == GLFW_PRESS)
      return;

    if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS) {
      leftMouseActiveVal = true;
      lastLeftPressedFrame = currentFrame;
    }
    if (button == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_PRESS) {
      lastRightPressedFrame = currentFrame;
    }

    if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_RELEASE) {
      leftMouseActiveVal = false;
    }
  }

  // Updates the screen width and height, in screen coordinates
  // (not necessarily the same as pixels)
  virtual void windowSizeCallback(int width, int height) {
    screenWidth = width;
    screenHeight = height;
  }

  // Sets the new cursor position, in screen coordinates
  virtual void cursorPosCallback(double xpos, double ypos) {
    screenMouseX = xpos;
    screenMouseY = ypos;
  }

  // Whether the left mouse was pressed down this frame.
  bool leftMouseJustPressed() { return lastLeftPressedFrame == currentFrame; }

  // Whether the left mouse button is being pressed down at all.
  bool leftMouseActive() { return leftMouseActiveVal; }

  // Whether the right mouse button was pressed down this frame.
  bool rightMouseJustPressed() { return lastRightPressedFrame == currentFrame; }

  // Tell the callbacks object a new frame has begun.
  void incrementFrameCount() { currentFrame++; }

  // Converts the cursor position from screen coordinates to GL coordinates
  // and returns the result.
  glm::vec2 getCursorPosGL() {
    glm::vec2 screenPos(screenMouseX, screenMouseY);
    // Interpret click as at centre of pixel.
    glm::vec2 centredPos = screenPos + glm::vec2(0.5f, 0.5f);
    // Scale cursor position to [0, 1] range.
    glm::vec2 scaledToZeroOne =
        centredPos / glm::vec2(screenWidth, screenHeight);

    glm::vec2 flippedY = glm::vec2(scaledToZeroOne.x, 1.0f - scaledToZeroOne.y);

    // Go from [0, 1] range to [-1, 1] range.
    return 2.f * flippedY - glm::vec2(1.f, 1.f);
  }

  // Takes in a list of points, given in GL's coordinate system,
  // and a threshold (in screen coordinates)
  // and then returns the index of the first point within that distance from
  // the cursor.
  // Returns -1 if no such point is found.
  int indexOfPointAtCursorPos(
      const std::vector<glm::vec3> &glCoordsOfPointsToSearch,
      float screenCoordThreshold) {
    // First, we conver thte points from GL to screen coordinates.
    std::vector<glm::vec3> screenCoordVerts;
    for (const auto &v : glCoordsOfPointsToSearch) {
      screenCoordVerts.push_back(glm::vec3(glPosToScreenCoords(v), 0.f));
    }

    // We make sure we interpret the cursor position as at the centre of
    // the relevant pixel, for consistency with getCursorPosGL().
    glm::vec3 cursorPosScreen(screenMouseX + 0.5f, screenMouseY + 0.5f, 0.f);

    for (size_t i = 0; i < screenCoordVerts.size(); i++) {
      // Return i if length of difference vector within threshold.
      glm::vec3 diff = screenCoordVerts[i] - cursorPosScreen;
      if (glm::length(diff) < screenCoordThreshold) {
        return i;
      }
    }
    return -1; // No point within threshold found.
  }

private:
  int screenWidth;
  int screenHeight;

  double screenMouseX;
  double screenMouseY;

  int currentFrame;

  bool leftMouseActiveVal;

  int lastLeftPressedFrame;
  int lastRightPressedFrame;

  // Converts GL coordinates to screen coordinates.
  glm::vec2 glPosToScreenCoords(glm::vec2 glPos) {
    // Convert the [-1, 1] range to [0, 1]
    glm::vec2 scaledZeroOne = 0.5f * (glPos + glm::vec2(1.f, 1.f));

    glm::vec2 flippedY = glm::vec2(scaledZeroOne.x, 1.0f - scaledZeroOne.y);
    glm::vec2 screenPos = flippedY * glm::vec2(screenWidth, screenHeight);
    return screenPos;
  }
};

int main(int argc, char *argv[]) {
  Log::debug("Starting main");

  // Validating input
  if (argc != 2) {
		Log::error("Enter options json file as command line argument");
    return 1;
  }
	std::string filename = argv[1];
	json opt_data = json::parse(std::ifstream(filename.c_str()));

  // WINDOW
  glfwInit();
  Window window(800, 800, "TreePanel (PANEL)"); // could set callbacks at
                                                // construction if desired

  GLDebug::enable();

  // SHADERS
  auto cb = std::make_shared<MyCallbacks>(window.getWidth(), window.getHeight());
  // CALLBACKS
  window.setCallbacks(cb);
  window.setupImGui(); // Make sure this call comes AFTER GLFW callbacks
                       // set.

  // RENDER LOOP
  while (!window.shouldClose()) {

    // Tell callbacks object a new frame's begun BEFORE polling events!
    cb->incrementFrameCount();
    glfwPollEvents();
    // Three functions that must be called each new frame.
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

		ImGui::SetNextWindowPos(ImVec2(0,0));
		ImGui::SetNextWindowSize(ImVec2((float)window.getWidth(),(float)window.getHeight()));

    ImGui::Begin("Tree Strands Options Panel", NULL, ImGuiWindowFlags_NoTitleBar|ImGuiWindowFlags_NoResize);

		ImGui::Text("Tree Strands Options Panel.");
		ImGui::Separator();

		json_to_gui(opt_data, "Options");

		if(ImGui::Button("Write To File")){
			std::ofstream opt_file(filename.c_str());
			if (!opt_file.is_open()){
				Log::error("Cannot edit options file");
			}
			opt_file<<opt_data<<std::endl;
			opt_file.close();
		}

    ImGui::End();
    ImGui::Render();

    glEnable(GL_FRAMEBUFFER_SRGB);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    // Draw Calls
    glDisable(GL_FRAMEBUFFER_SRGB); // disable sRGB for things like imgui
ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData()); window.swapBuffers();
  }

  // Cleanup
  ImGui_ImplOpenGL3_Shutdown();
  ImGui_ImplGlfw_Shutdown();
  ImGui::DestroyContext();

  glfwTerminate();
  return 0;
}
void json_to_gui(json &option, std::string name) {
		if (option.is_object()){
				if(!ImGui::CollapsingHeader(name.c_str())){
					for (json::iterator it = option.begin(); it != option.end(); ++it) {
						json_to_gui(it.value(), it.key());
					}
					ImGui::Separator();
					ImGui::Spacing();
					ImGui::Spacing();
				}
		}else if(option.is_number_integer()){
			auto max_range = get_range_i(name);
			int min = max_range.first;
			int max = max_range.second;
			int temp = option.get<int>();
			ImGui::SliderInt(name.c_str(),&temp,min,max);
			option = temp;
		}else if(option.is_number_float()){
			auto max_range = get_range_f(name);
			float min = max_range.first;
			float max = max_range.second;
			float temp = option.get<float>();
			ImGui::SliderFloat(name.c_str(),&temp,min,max);
			option = temp;
		}else if(option.is_string()){

		}else if(option.is_boolean()){
			bool temp = option.get<bool>();
			ImGui::Checkbox(name.c_str(),&temp);
			option = temp;
		}
}

std::pair<float, float> get_range_f(std::string name) {
	if (name == "max_val") {
		return std::make_pair(0.1f, 20.0f);
	} else if (name == "range") {
		return std::make_pair(0.001f, 0.1f);
	} else if (name == "local_spread") {
		return std::make_pair(0.001f, 0.1f);
	} else if (name == "max_angle") {
		return std::make_pair(0.0f, 360.0f);
	} else if (name == "segment_length"){
		return std::make_pair(0.001f, 0.1f);
	}else if(name.find("eval") != std::string::npos){
		return std::make_pair(0.f, 10.0f);
	} else if (name.find("iso") != std::string::npos) {
		return std::make_pair(0.0f, 50.0f);
    }
	return std::make_pair(0.f, 1.0f);
}
std::pair<int, int> get_range_i(std::string name) {
	if (name == "num_per"){
		return std::make_pair(0,10);
	}else if (name == "num_abs"){
		return std::make_pair(0,200);
	}else if (name == "num_trials"){
		return std::make_pair(1,200);
	}
	return std::make_pair(0,10);
}
