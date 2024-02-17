#define GL_SILENCE_DEPRECATION
#define STB_IMAGE_IMPLEMENTATION

#ifdef _WINDOWS
#include <GL/glew.h>
#endif

#define GL_GLEXT_PROTOTYPES 1
#include <SDL.h>
#include <SDL_opengl.h>
#include "glm/mat4x4.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "ShaderProgram.h"
#include "stb_image.h"

// window size
const int WINDOW_WIDTH = 640,
		  WINDOW_HEIGHT = 480;

// background color
const float BG_RED = 0.84f,
			BG_GREEN = 0.68f,
			BG_BLUE = 0.39f,
			BG_OPACITY = 1.0f;

// camera position and size
const int VIEWPORT_X = 0,
		  VIEWPORT_Y = 0,
		  VIEWPORT_WIDTH = WINDOW_WIDTH,
		  VIEWPORT_HEIGHT = WINDOW_HEIGHT;

// paths for shaders and sprites
const char V_SHADER_PATH[] = "shaders/vertex_textured.glsl",
		   F_SHADER_PATH[] = "shaders/fragment_textured.glsl",
		   BREEZE_PATH[] = "assets/breeze_thin.png",
		   WINDBALL_PATH[] = "assets/wind_charge.png";

// const for deltaTime calc
const float MILLISECONDS_IN_SECOND = 1000.0;

// texture constants
const int NUMBER_OF_TEXTURES = 1; // to be generated, that is
const GLint LEVEL_OF_DETAIL = 0; // base image level; Level n is the nth mipmap reduction image
const GLint TEXTURE_BORDER = 0; // this value MUST be zero

// shader and associated matrices
ShaderProgram g_shaderProgram;
glm::mat4 g_viewMatrix,
		  g_modelMatrix_p1,
		  g_modelMatrix_p2,
	      g_modelMatrix_ball,
		  g_projectionMatrix;

// core globals
SDL_Window* g_displayWindow;
bool g_gameIsRunning = true;
float g_previousTicks;

// custom globals
GLuint g_player1TextureID;
GLuint g_player2TextureID;
GLuint g_windballTextureID;

glm::vec3 g_player1Pos = glm::vec3(-4.5f, 0.0f, 0.0f);
glm::vec3 g_player2Pos = glm::vec3(4.5f, 0.0f, 0.0f);
glm::vec3 g_windballPos = glm::vec3(0.0f);

glm::vec3 g_player1Dir = glm::vec3(0.0f);
glm::vec3 g_player2Dir = glm::vec3(0.0f);
glm::vec3 g_windballDir = glm::vec3(-1.0f, 0.0f, 0.0f);

float g_timer = 0.0;
bool g_vsAI = false;
bool g_AImovingUp = true;

GLuint load_texture(const char* filepath) {
	// load image file
	int width, height, numOfComponents;
	unsigned char* image = stbi_load(filepath, &width, &height, &numOfComponents, STBI_rgb_alpha);
	if (image == NULL) {
		std::cout << "Unable to load image. Provided path '" << filepath << "' may be incorrect." << std::endl;
		assert(false);
	}
	// generate and bind texture ID
	GLuint textureID;
	glGenTextures(NUMBER_OF_TEXTURES, &textureID);
	glBindTexture(GL_TEXTURE_2D, textureID);
	glTexImage2D(GL_TEXTURE_2D, LEVEL_OF_DETAIL, GL_RGBA, width, height, TEXTURE_BORDER, GL_RGBA, GL_UNSIGNED_BYTE, image);

	// set filter parameters
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	// release image from memory and return the bound ID
	stbi_image_free(image);
	return textureID;
}

void initialize() {
	SDL_Init(SDL_INIT_VIDEO);
	g_displayWindow = SDL_CreateWindow("Breeze pong!", 
									   SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 
									   WINDOW_WIDTH, WINDOW_HEIGHT, 
									   SDL_WINDOW_OPENGL);
	SDL_GLContext context = SDL_GL_CreateContext(g_displayWindow);
	SDL_GL_MakeCurrent(g_displayWindow, context);

#ifdef _WINDOWS
	glewInit();
#endif

	glViewport(0, 0, 640, 480);

	g_shaderProgram.load(V_SHADER_PATH, F_SHADER_PATH);

	// load the textures into IDs here!
	g_player1TextureID = load_texture(BREEZE_PATH);
	g_player2TextureID = load_texture(BREEZE_PATH);
	g_windballTextureID = load_texture(WINDBALL_PATH);

	g_viewMatrix = glm::mat4(1.0f);
	g_modelMatrix_p1 = glm::mat4(1.0f);
	g_modelMatrix_p2 = glm::mat4(1.0f);
	g_modelMatrix_ball = glm::mat4(1.0f);
	g_projectionMatrix = glm::ortho(-5.0f, 5.0f, -3.75f, 3.75f, -1.0f, 1.0f);

	g_shaderProgram.set_projection_matrix(g_projectionMatrix);
	g_shaderProgram.set_view_matrix(g_viewMatrix);

	glUseProgram(g_shaderProgram.get_program_id());

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	glClearColor(BG_RED, BG_GREEN, BG_BLUE, BG_OPACITY);
}

void processInput() {
	// reset player movement directions
	g_player1Dir = glm::vec3(0.0f);
	g_player2Dir = glm::vec3(0.0f);

	// poll for event triggers
	SDL_Event event;
	while (SDL_PollEvent(&event)) {
		if (event.type == SDL_QUIT || event.type == SDL_WINDOWEVENT_CLOSE) {
			g_gameIsRunning = false;
		} else if (event.type == SDL_KEYDOWN) {
			switch (event.key.keysym.sym) {
				case SDLK_q:
					g_gameIsRunning = false;
					break;
				case SDLK_t:
					g_vsAI = !g_vsAI;
					break;
			}
		} 
	}

	// respond to player movement inputs
	const Uint8* key_state = SDL_GetKeyboardState(NULL);
	if (key_state[SDL_SCANCODE_W]) {
		g_player1Dir.y += 1.0f;
	}
	if (key_state[SDL_SCANCODE_S]) {
		g_player1Dir.y += -1.0f;
	}
	if (key_state[SDL_SCANCODE_UP] && !g_vsAI) {
		g_player2Dir.y += 1.0f;
	}
	if (key_state[SDL_SCANCODE_DOWN] && !g_vsAI) {
		g_player2Dir.y += -1.0f;
	}
}

void update() {
	float ticks = (float)SDL_GetTicks() / MILLISECONDS_IN_SECOND; // get the current number of ticks
	float deltaTime = ticks - g_previousTicks; // the delta time is the difference from the last frame
	g_previousTicks = ticks;

	// if player 2 is AI-controlled, determine their motion
	if (g_vsAI) {
		if (g_AImovingUp) {
			if (g_player2Pos.y >= 2.55f) {
				g_AImovingUp = false;
				g_player2Dir.y = -1.0f;
			} else {
				g_player2Dir.y = 1.0f;
			}
		} else {
			if (g_player2Pos.y <= -2.55f) {
				g_AImovingUp = true;
				g_player2Dir.y = 1.0f;
			}
			else {
				g_player2Dir.y = -1.0f;
			}
		}
	}

	// apply motion
	g_player1Pos += g_player1Dir * 3.0f * deltaTime;
	g_player2Pos += g_player2Dir * 3.0f * deltaTime;
	g_windballPos += g_windballDir * 2.0f * deltaTime;

	// reset and translate all the objects
	g_modelMatrix_p1 = glm::mat4(1.0f);
	g_modelMatrix_p2 = glm::mat4(1.0f);
	g_modelMatrix_ball = glm::mat4(1.0f);
	g_modelMatrix_p1 = glm::translate(g_modelMatrix_p1, g_player1Pos);
	g_modelMatrix_p2 = glm::translate(g_modelMatrix_p2, g_player2Pos);
	g_modelMatrix_ball = glm::translate(g_modelMatrix_ball, g_windballPos);

	// scale everything to the correct shape
	g_modelMatrix_p1 = glm::scale(g_modelMatrix_p1, glm::vec3(-1.1f, 2.75f, 0.0f));
	g_modelMatrix_p2 = glm::scale(g_modelMatrix_p2, glm::vec3(1.1f, 2.75f, 0.0f));
	g_modelMatrix_ball = glm::scale(g_modelMatrix_ball, glm::vec3(0.8f, 0.8f, 0.0f));
}

void draw_object(glm::mat4& modelMatrix, GLuint& textureID) {
	g_shaderProgram.set_model_matrix(modelMatrix);
	glBindTexture(GL_TEXTURE_2D, textureID);
	glDrawArrays(GL_TRIANGLES, 0, 6);
}

void render() {
	glClear(GL_COLOR_BUFFER_BIT);

	// vertices for square sprite
	float vertices[] = {
		-0.5f, -0.5f, 0.5f, -0.5f, 0.5f, 0.5f,  // triangle 1
		-0.5f, -0.5f, 0.5f, 0.5f, -0.5f, 0.5f   // triangle 2
	};

	// vertices for texture coords
	float texture_coordinates[] = {
		0.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.0f,     // triangle 1
		0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 0.0f,     // triangle 2
	};

	glVertexAttribPointer(g_shaderProgram.get_position_attribute(), 2, GL_FLOAT, false, 0, vertices);
	glEnableVertexAttribArray(g_shaderProgram.get_position_attribute());

	glVertexAttribPointer(g_shaderProgram.get_tex_coordinate_attribute(), 2, GL_FLOAT, false, 0, texture_coordinates);
	glEnableVertexAttribArray(g_shaderProgram.get_tex_coordinate_attribute());

	// draw the sprites here!
	draw_object(g_modelMatrix_p1, g_player1TextureID);
	draw_object(g_modelMatrix_p2, g_player2TextureID);
	draw_object(g_modelMatrix_ball, g_windballTextureID);

	glDisableVertexAttribArray(g_shaderProgram.get_position_attribute());
	glDisableVertexAttribArray(g_shaderProgram.get_tex_coordinate_attribute());

	SDL_GL_SwapWindow(g_displayWindow);
}

void shutdown() {
	SDL_Quit();
}

int main(int argc, char* argv[]) {
	initialize();
	
	while (g_gameIsRunning) {
		processInput();
		update();
		render();
	}

	shutdown();
	return 0;
}