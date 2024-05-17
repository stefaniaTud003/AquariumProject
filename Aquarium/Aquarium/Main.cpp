﻿#define GLM_FORCE_CTOR_INIT 

#include <GL/glew.h>
#include <glfw3.h>
#include <chrono>

#include "Camera.h"
#include "Shader.h"
#include "Mesh.h"
#include "Model.h"
#include <stb_image.h>

#include "Fish.h"

#pragma comment (lib, "glfw3dll.lib")
#pragma comment (lib, "glew32.lib")
#pragma comment (lib, "OpenGL32.lib")

// settings
const unsigned int SCR_WIDTH = 2560;
const unsigned int SCR_HEIGHT = 1440;

auto t_start = std::chrono::high_resolution_clock::now();

Camera* pCamera;
std::unique_ptr<Mesh> floorObj, cubeObj;
std::unique_ptr<Model> fishObj, goldFishObj, coralBeautyFishObj, grayFishObj, starFishObj, bubbleObj, sandDune, coral, plant, anchor;
float timeAcceleration = 0.1f;
glm::vec3 zrotation = glm::vec3(0.0f, 0.0f, 0.0f);

void FramebufferSizeCallback(GLFWwindow* window, int width, int height);
void MouseCallback(GLFWwindow* window, double xpos, double ypos);
void ScrollCallback(GLFWwindow* window, double xoffset, double yoffset);
void KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
void ProcessKeyboard(GLFWwindow* window);

void LoadObjects();
void RenderScene(Shader& shader);


double deltaTime = 0.0f;
double lastFrame = 0.0f;
int m_mapWidth, m_mapHeight, m_mapChannels, m_indicesRez;
std::vector<float> m_vertices;
int numGreyFishes = 20;
int numBubbles = 20;
struct BubbleParams
{
	glm::vec3 position;
	glm::vec3 newPos;
	float size;
	float speed;
	float startTime;
	float radius;
};
float verticalSpeed = 0.2f;
float bubbleTime = 0.0f;
std::vector<BubbleParams> bubbles;

void DrawColoredBorder(Model& fishObjModel, const glm::mat4& fishModel, bool transparent) {
	const glm::vec3 borderColor = glm::vec3(1.0f, 0.0f, 0.0f); // Red color

	glm::mat4 borderModel = fishModel;
	Shader borderShader("../Shaders/Border.vs", "../Shaders/Border.fs");
	borderShader.Use();

	borderShader.SetMat4("model", borderModel);
	borderShader.SetMat4("view", pCamera->GetViewMatrix());
	borderShader.SetMat4("projection", pCamera->GetProjectionMatrix());
	borderShader.SetVec3("borderColor", borderColor);
	if (transparent) {
		borderShader.SetVec4("borderColor", glm::vec4(borderColor, 0.0f));
	}
	glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	fishObjModel.RenderModel(borderShader);
	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
}

void SwitchToFishPerspective(Camera* camera, const glm::vec3& fishPosition)
{
	glm::vec3 offset = glm::vec3(0.0f, 1.5f, -3.0f);
	camera->SetPosition(fishPosition + offset);
	glm::vec3 direction = glm::normalize(fishPosition - camera->GetPosition());

	float yaw = glm::degrees(atan2(direction.x, direction.z)) + 90;
	float pitch = glm::degrees(asin(direction.y));

	camera->SetYaw(yaw);
	camera->SetPitch(pitch);

}

void generateBubblesParams()
{
	for (int i = 0; i < numBubbles; i++)
	{
		BubbleParams bubble;
		bubble.position = glm::vec3(static_cast<float>(rand()) / (static_cast<float>(RAND_MAX / 0.1f)));
		bubble.size = rand() % 10 / 100.0f;
		bubble.speed = rand() % 10 / 100.0f;
		bubble.startTime = rand() % 10;
		bubble.radius = rand() % 10 / 10.0f;
		bubbles.push_back(bubble);
	}
}


void generateBubbleParams(int index)
{
	bubbles[index].position = glm::vec3(static_cast<float>(rand()) / (static_cast<float>(RAND_MAX / 0.1f)));
	bubbles[index].size = rand() % 10 / 100.0f;
	bubbles[index].speed = rand() % 10 / 100.0f;
	bubbles[index].startTime = 0;
	bubbles[index].radius = rand() % 10 / 10.0f;
}

// Define initial position of the bubble
glm::vec3 bubblePosition;

void resetBubblePosition(int index) {
	bubbles[index].position = bubbles[index].newPos;
	bubbles[index].size = rand() % 10 / 100.0f;
	bubbles[index].startTime = 0.0f;
}

// Function to update the position of the bubble with an offset
void updateBubblePosition(int index) {
	static std::chrono::steady_clock::time_point lastUpdateTime = std::chrono::steady_clock::now();
	std::chrono::steady_clock::time_point currentTime = std::chrono::steady_clock::now();
	std::chrono::duration<double> elapsedTime = currentTime - lastUpdateTime;

	// If 2 seconds have passed, update the bubble position
	lastUpdateTime = currentTime;

	float xSpiral = 0.5f * cos(bubbles[index].speed * bubbles[index].startTime); // Compute x-coordinate of spiral
	float zSpiral = 0.5f * sin(bubbles[index].speed * bubbles[index].startTime); // Compute z-coordinate of spiral
	float y = verticalSpeed * bubbles[index].startTime; // Compute y-coordinate for vertical motion
	bubbles[index].size -= 0.0001f;
	bubbles[index].startTime += 0.01f;

	if (bubbles[index].size <= 0.0f)
	{
		if (elapsedTime.count() < 2.0)
		{
			resetBubblePosition(index);
			return;
		}
	}
	bubbles[index].position = bubbles[index].newPos + glm::vec3(xSpiral, y, zSpiral);
}

std::vector<Fish> fishes;

void InitFishParams()
{
	for (int i = 0; i < numGreyFishes; i++)
	{
		Fish fish;
		fish.SetPos(glm::vec3(static_cast<float>(rand()) / (static_cast<float>(RAND_MAX / 0.5f))));
		fish.SetFishSize(0.1f);
		float timer = 0.0f;
		fish.SetFishMovementTimer(timer);
		fishes.push_back(fish);
	}
}

void resetFishTimer(int index)
{
	fishes[index].SetFishMovementTimer(1.0f + static_cast<float>(rand()) / (static_cast<float>(RAND_MAX / (5.0f - 2.0f))));
}

void UpdateFishPosition(int index, EFishMovementType direction)
{
	// Move the fish in the specified direction

	fishes[index].Move(direction);
	fishes[index].MoveFish(deltaTime);
	float currFishTimer = fishes[index].GetFishMovementTimer();

	while (currFishTimer <= 0)
	{
		resetFishTimer(index);
		currFishTimer = fishes[index].GetFishMovementTimer();
		//return;
	}
	fishes[index].SetFishMovementTimer(currFishTimer - 0.01f);
}


float roiRadius = 5.0f;
glm::vec3 fishPosition = glm::vec3(0.0f, 3.0f, 0.0f);

// Check if camera is within ROI
bool IsCameraWithinROI(Camera* camera, const glm::vec3& fishPosition, float roiRadius) {
	// Calculate distance between camera position and fish position
	glm::vec3 cameraPosition = camera->GetPosition();
	float distance = glm::distance(cameraPosition, fishPosition);
	return distance < roiRadius;
}

int keyPress = 0;
bool isInFishPerspective = false;
bool isTransparent = false;

glm::mat4 fishModel = glm::mat4(1.0f);
void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
	if (key == GLFW_KEY_A && action == GLFW_PRESS) {

	}
	// Check if F key is pressed to switch camera perspective
	if (key == GLFW_KEY_F && action == GLFW_PRESS) {
		// Check if camera is within ROI of the fish
		if (IsCameraWithinROI(pCamera, fishPosition, roiRadius)) {
			// Switch camera perspective to that of the fish
			if (keyPress % 2 == 0) {
				SwitchToFishPerspective(pCamera, fishPosition);
				isInFishPerspective = true;
			}
			else {
				// Return to initial camera position
				pCamera->SetPosition(glm::vec3(0.0, 1.0, 3.0));
				isInFishPerspective = false;
			}
			isTransparent = !isTransparent;
			keyPress++;
		}
		else if (keyPress % 2 == 1) {
			// Return to initial camera position
			pCamera->SetPosition(glm::vec3(0.0, 1.0, 3.0));
			keyPress++;
			isInFishPerspective = false;
		}
	}
}

float fishAngleY = 0.0f;
float fishAngleZ = 0.0f;

Fish greyFish(glm::vec3(0.0f, 3.0f, 0.0f));

int main(int argc, char** argv)
{
	glfwInit();
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

	GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "blip blop Simulator", NULL, NULL);

	if (window == NULL) {
		std::cout << "Failed to create GLFW window" << std::endl;
		glfwTerminate();
		return -1;
	}

	glfwMakeContextCurrent(window);
	glfwSetFramebufferSizeCallback(window, FramebufferSizeCallback);
	glfwSetCursorPosCallback(window, MouseCallback);
	glfwSetScrollCallback(window, ScrollCallback);
	glfwSetKeyCallback(window, KeyCallback);

	glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

	glewInit();

	pCamera = new Camera(SCR_WIDTH, SCR_HEIGHT, glm::vec3(0.0, 20, 0));
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	//glEnable(GL_CULL_FACE);
	glEnable(GL_COLOR_MATERIAL);
	glDisable(GL_LIGHTING);
	glFrontFace(GL_CCW);
	//glCullFace(GL_BACK);

	Shader shadowMappingShader("../Shaders/ShadowMapping.vs", "../Shaders/ShadowMapping.fs");
	Shader shadowMappingDepthShader("../Shaders/ShadowMappingDepth.vs", "../Shaders/ShadowMappingDepth.fs");
	Shader borderShader("../Shaders/Border.vs", "../Shaders/Border.fs");
	const unsigned int SHADOW_WIDTH = 4000, SHADOW_HEIGHT = 4000;
	unsigned int depthMapFBO;
	glGenFramebuffers(1, &depthMapFBO);
	// create depth texture
	unsigned int depthMap;
	glGenTextures(1, &depthMap);
	glBindTexture(GL_TEXTURE_2D, depthMap);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, SHADOW_WIDTH, SHADOW_HEIGHT, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);

	float borderColor[] = { 1.0, 1.0, 1.0, 1.0 };
	glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);
	glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthMap, 0);
	glDrawBuffer(GL_NONE);
	glReadBuffer(GL_NONE);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	// shader configuration
	shadowMappingShader.Use();
	shadowMappingShader.SetInt("diffuseTexture", 0);
	shadowMappingShader.SetInt("shadowMap", 1);

	LoadObjects();

	glm::vec3 lightPos(-2.0f, 14.0f, -1.0f);
	float hue = 1.0;
	float floorHue = 0.9;
	bool rotateLight = true;
	generateBubblesParams();
	InitFishParams();
	while (!glfwWindowShouldClose(window))
	{
		// per-frame time logic
		// --------------------
		float currentFrame = (float)glfwGetTime();
		deltaTime = currentFrame - lastFrame;
		lastFrame = currentFrame;

		// input
		// -----
		ProcessKeyboard(window);

		// render
		// ------
		glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

		// 1. render depth of scene to texture (from light's perspective)
		glm::mat4 lightProjection, lightView;
		glm::mat4 lightSpaceMatrix;
		float near_plane = 5.0f, far_plane = 500.f;

		if (rotateLight) {
			float radius = 2.0f;
			float time = glfwGetTime();
			float lightX = cos(time) * radius;
			float lightZ = sin(time) * radius;
			lightPos = glm::vec3(lightX, 14.0f, lightZ);
		}

		lightProjection = glm::ortho(-500.0f, 500.0f, -500.0f, 500.0f, near_plane, far_plane);
		lightView = glm::lookAt(lightPos, glm::vec3(0.0f), glm::vec3(0.0, 1.0, 0.0));
		lightSpaceMatrix = lightProjection * lightView;

		// render scene from light's point of view
		shadowMappingDepthShader.Use();
		shadowMappingDepthShader.SetMat4("lightSpaceMatrix", lightSpaceMatrix);

		glViewport(0, 0, SHADOW_WIDTH, SHADOW_HEIGHT);
		glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO);
		glClear(GL_DEPTH_BUFFER_BIT);
		RenderScene(shadowMappingDepthShader);
		glActiveTexture(GL_TEXTURE0);
		glBindFramebuffer(GL_FRAMEBUFFER, 0);

		// reset viewport
		glViewport(0, 0, SCR_WIDTH, SCR_HEIGHT);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		// 2. render scene as normal using the generated depth/shadow map 
		glViewport(0, 0, SCR_WIDTH, SCR_HEIGHT);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		shadowMappingShader.Use();
		glm::mat4 projection = pCamera->GetProjectionMatrix();
		glm::mat4 view = pCamera->GetViewMatrix();
		shadowMappingShader.SetMat4("projection", projection);
		shadowMappingShader.SetMat4("view", view);
		shadowMappingShader.SetFloat("hue", floorHue);


		// set light uniforms
		shadowMappingShader.SetVec3("viewPos", pCamera->GetPosition());
		shadowMappingShader.SetVec3("lightPos", lightPos);
		shadowMappingShader.SetMat4("lightSpaceMatrix", lightSpaceMatrix);
		glActiveTexture(GL_TEXTURE0);
		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D, depthMap);
		glDisable(GL_CULL_FACE);
		RenderScene(shadowMappingShader);

		/*float sunPassingTime = currentFrame * timeAcceleration;
		lightPos = glm::vec3(0.0f, 20 * sin(sunPassingTime), 50 * cos(sunPassingTime));*/
		//hue = std::max<float>(sin(sunPassingTime), 0.1);
		//floorHue = std::max<float>(sin(sunPassingTime), 0.6);
		if (IsCameraWithinROI(pCamera, fishPosition, roiRadius) && !isInFishPerspective)
		{
			const glm::vec3 borderColor = glm::vec3(1.0f, 0.0f, 0.0f); // Red color

			glm::mat4 borderModel = fishModel;
			Shader borderShader("../Shaders/Border.vs", "../Shaders/Border.fs");
			borderShader.Use();

			borderShader.SetMat4("model", borderModel);
			borderShader.SetMat4("view", pCamera->GetViewMatrix());
			borderShader.SetMat4("projection", pCamera->GetProjectionMatrix());
			borderShader.SetVec3("borderColor", borderColor);
			if (isTransparent) {
				borderShader.SetVec4("borderColor", glm::vec4(borderColor, 0.0f));
			}
			glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
			fishObj->RenderModel(borderShader);
			glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
		}


		hue = 1;
		floorHue = 1;
		glfwSwapBuffers(window);
		glfwPollEvents();
	}

	glfwTerminate();
	return 0;
}

void ProcessKeyboard(GLFWwindow* window)
{
	if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
		glfwSetWindowShouldClose(window, true);

	if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
		pCamera->ProcessKeyboard(FORWARD, deltaTime);
	if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
		pCamera->ProcessKeyboard(BACKWARD, deltaTime);
	if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
		pCamera->ProcessKeyboard(LEFT, deltaTime);
	if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
		pCamera->ProcessKeyboard(RIGHT, deltaTime);
	if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS)
		pCamera->ProcessKeyboard(UP, deltaTime);
	if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS)
		pCamera->ProcessKeyboard(DOWN, deltaTime);


	if (glfwGetKey(window, GLFW_KEY_R) == GLFW_PRESS) {
		int width, height;
		glfwGetWindowSize(window, &width, &height);
		pCamera->Reset(width, height);
	}
}

void LoadObjects()
{
	// Texture loading
	Texture floorTexture("../Models/Floor/FloorTexture.jpg");
	Texture cubeTexture("../Models/Cube/glass.png");
	const float floorSize = 5000.0f;
	std::vector<Vertex> floorVertices({
		// positions            // normals           // texcoords
		{ floorSize, -13.0f,  floorSize,  0.0f, 1.0f, 0.0f,    floorSize,  0.0f},
		{-floorSize, -13.0f,  floorSize,  0.0f, 1.0f, 0.0f,    0.0f,  0.0f},
		{-floorSize, -13.0f, -floorSize,  0.0f, 1.0f, 0.0f,    0.0f, floorSize},

		{ floorSize, -13.0f,  floorSize,  0.0f, 1.0f, 0.0f,    floorSize,  0.0f},
		{-floorSize, -13.0f, -floorSize,  0.0f, 1.0f, 0.0f,    0.0f, floorSize},
		{ floorSize, -13.0f, -floorSize,  0.0f, 1.0f, 0.0f,    floorSize, floorSize}
		});

	std::vector<Vertex> cubeVertices({
		// Back face
		Vertex(-12.5f, -12.5f, -12.5f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f),
		Vertex(12.5f, -12.5f, -12.5f, 0.0f, 0.0f, -1.0f, 1.0f, 0.0f),
		Vertex(12.5f, 12.5f, -12.5f, 0.0f, 0.0f, -1.0f, 1.0f, 1.0f),
		Vertex(12.5f, 12.5f, -12.5f, 0.0f, 0.0f, -1.0f, 1.0f, 1.0f),
		Vertex(-12.5f, 12.5f, -12.5f, 0.0f, 0.0f, -1.0f, 0.0f, 1.0f),
		Vertex(-12.5f, -12.5f, -12.5f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f),

		// Front face
		Vertex(-12.5f, -12.5f, 12.5f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f),
		Vertex(12.5f, -12.5f, 12.5f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f),
		Vertex(12.5f, 12.5f, 12.5f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f),
		Vertex(12.5f, 12.5f, 12.5f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f),
		Vertex(-12.5f, 12.5f, 12.5f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f),
		Vertex(-12.5f, -12.5f, 12.5f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f),

		// Left face
		Vertex(-12.5f, 12.5f, 12.5f, -1.0f, 0.0f, 0.0f, 0.0f, 0.0f),
		Vertex(-12.5f, 12.5f, -12.5f, -1.0f, 0.0f, 0.0f, 1.0f, 0.0f),
		Vertex(-12.5f, -12.5f, -12.5f, -1.0f, 0.0f, 0.0f, 1.0f, 1.0f),
		Vertex(-12.5f, -12.5f, -12.5f, -1.0f, 0.0f, 0.0f, 1.0f, 1.0f),
		Vertex(-12.5f, -12.5f, 12.5f, -1.0f, 0.0f, 0.0f, 0.0f, 1.0f),
		Vertex(-12.5f, 12.5f, 12.5f, -1.0f, 0.0f, 0.0f, 0.0f, 0.0f),

		// Right face
		Vertex(12.5f, 12.5f, 12.5f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f),
		Vertex(12.5f, -12.5f, 12.5f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f),
		Vertex(12.5f, -12.5f, -12.5f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f),
		Vertex(12.5f, -12.5f, -12.5f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f),
		Vertex(12.5f, 12.5f, -12.5f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f),
		Vertex(12.5f, 12.5f, 12.5f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f),

		// Bottom face
		Vertex(-12.5f, -12.5f, -12.5f, 0.0f, -1.0f, 0.0f, 0.0f, 0.0f),
		Vertex(12.5f, -12.5f, -12.5f, 0.0f, -1.0f, 0.0f, 1.0f, 0.0f),
		Vertex(12.5f, -12.5f, 12.5f, 0.0f, -1.0f, 0.0f, 1.0f, 1.0f),
		Vertex(12.5f, -12.5f, 12.5f, 0.0f, -1.0f, 0.0f, 1.0f, 1.0f),
		Vertex(-12.5f, -12.5f, 12.5f, 0.0f, -1.0f, 0.0f, 0.0f, 1.0f),
		Vertex(-12.5f, -12.5f, -12.5f, 0.0f, -1.0f, 0.0f, 0.0f, 0.0f)
		});

	stbi_set_flip_vertically_on_load(false);



	// Objects loading
	cubeObj = std::make_unique<Mesh>(cubeVertices, std::vector<unsigned int>(), std::vector<Texture>{cubeTexture});
	fishObj = std::make_unique<Model>("../Models/Fish/fish.obj", false);
	goldFishObj = std::make_unique<Model>("../Models/GoldFish/fish2.obj", false);
	coralBeautyFishObj = std::make_unique<Model>("../Models/CoralBeauty/coralBeauty.obj", false);
	grayFishObj = std::make_unique<Model>("../Models/GreyFish/fish.obj", false);
	starFishObj = std::make_unique<Model>("../Models/StarFish/starFish.obj", false);
	bubbleObj = std::make_unique<Model>("../Models/Bubble/bubble.obj", false);
	sandDune = std::make_unique<Model>("../Models/SandDune/sandDunes.obj", false);
	floorObj = std::make_unique<Mesh>(floorVertices, std::vector<unsigned int>(), std::vector<Texture>{floorTexture});
}
EFishMovementType direction = EFishMovementType::FORWARD;

int movementIndex = 0;

void RenderScene(Shader& shader)
{
	//glm::mat4 waterModelMatrix = glm::mat4(1.0f);
	//waterModelMatrix = glm::translate(waterModelMatrix, glm::vec3(0.0f, -12.f, 0.0f));
	//waterModelMatrix = glm::scale(waterModelMatrix, glm::vec3(0.14f, 1.f, 0.14f));
	////water->RenderModel(shader, waterModelMatrix);

	glm::mat4 goldFish = glm::mat4(1.0f);

	goldFish = glm::translate(goldFish, glm::vec3(10.0f, 3.0f, -1.0f));
	goldFish = glm::rotate(goldFish, glm::radians(90.0f), glm::vec3(-1.0f, 0.0f, 0.0f));
	//goldFish = glm::scale(goldFish, glm::vec3(0.1f, 0.1f, 0.1f));
	goldFishObj->RenderModel(shader, goldFish);


	glm::mat4 coralBeautyModelMatrix = glm::mat4(1.0f);
	coralBeautyModelMatrix = glm::translate(coralBeautyModelMatrix, glm::vec3(2.0f, 3.0f, -1.0f));
	coralBeautyModelMatrix = glm::rotate(coralBeautyModelMatrix, glm::radians(90.0f), glm::vec3(-1.0f, 0.0f, 0.0f));
	coralBeautyModelMatrix = glm::scale(coralBeautyModelMatrix, glm::vec3(0.8f, 0.8f, 0.8f));
	// Desenează modelul Coral Beauty
	coralBeautyFishObj->RenderModel(shader, coralBeautyModelMatrix);

	// Configurare model matrix pentru Starfish
	glm::mat4 starfishModelMatrix = glm::mat4(1.0f);
	starfishModelMatrix = glm::translate(starfishModelMatrix, glm::vec3(-1.0f, 0.0f, -1.0f)); // Ajustează poziționarea
	starfishModelMatrix = glm::scale(starfishModelMatrix, glm::vec3(0.1f, 0.1f, 0.1f)); // Ajustează scalarea

	// Desenează modelul Starfish
	starFishObj->RenderModel(shader, starfishModelMatrix);

	fishModel = glm::mat4(1.0f);
	fishModel = glm::translate(fishModel, fishPosition);
	//rotate around z-axis
	fishModel = glm::rotate(fishModel, glm::radians(
		90.0f), glm::vec3(0.0f, 0.0f, 1.0f)); // Rotate around z-axis
	fishModel = glm::rotate(fishModel, glm::radians(
		90.0f), glm::vec3(0.0f, 1.0f, 0.0f)); // Rotate around y-axis
	fishModel = glm::scale(fishModel, glm::vec3(
		0.1f, 0.1f, 0.1f
	)); // Scale uniformly


	// Define animation parameters
	float time = glfwGetTime(); // Get current time
	float fishSpeed = 1.0f; // Speed of fish animation

	// Define circle parameters
	float radius = 8.0f; // Radius of the circular path
	float angularSpeed = 0.1f; // Angular speed of the fish's movement

	// Number of fishes in the school
	float spacing = 0.2f; // Spacing between fishes

	// Loop to create the fish school
	for (int i = 0; i < numGreyFishes; ++i) {
		glm::mat4 greyFish = glm::mat4(1.0f); // Reset fish model matrix

		float currFishTimer = fishes[i].GetFishMovementTimer();
		if (currFishTimer <= 0) {
			resetFishTimer(i);
			direction = fishes[i].GetMove(movementIndex);
			movementIndex++;
			currFishTimer = fishes[i].GetFishMovementTimer();

			// Start a new movement with the current direction and total time
			fishes[i].StartNewMovement(currFishTimer, direction);
		}

		// Move the fish based on the current speed and direction
		float deltaTime = 0.1f; // Example delta time, adjust as needed
		fishes[i].MoveFish(deltaTime);

		// Update the remaining fish movement timer
		fishes[i].SetFishMovementTimer(currFishTimer - deltaTime);

		// Get the updated fish position
		auto pos = fishes[i].GetPos();

		// Update the fish's model matrix with position, rotation, and scale
		greyFish = glm::translate(greyFish, pos); // Set initial position of fish
		greyFish = glm::rotate(greyFish, glm::radians(90.0f), glm::vec3(0, 1, 0)); // Rotate fish to face forward
		greyFish = glm::rotate(greyFish, -glm::radians(fishes[i].GetYaw()), glm::vec3(0, 1, 0)); // Rotate fish based on yaw
		greyFish = glm::rotate(greyFish, -glm::radians(fishes[i].GetPitch()), glm::vec3(1, 0, 0)); // Rotate fish based on pitch
		greyFish = glm::scale(greyFish, glm::vec3(0.1f, 0.1f, 0.1f)); // Scale fish

		// Calculate new position for bubble
		glm::vec3 bubbleInitialPos = pos;

		// Render the fish object
		grayFishObj->RenderModel(shader, greyFish); // Draw fish object

		// Update the bubble position
		bubbles[i].newPos = bubbleInitialPos;

	}

	glm::mat4 sandDuneModelMatrix = glm::mat4(1.0f);
	sandDuneModelMatrix = glm::translate(sandDuneModelMatrix, glm::vec3(0.0f, -12.f, 0.0f)); 
	sandDuneModelMatrix = glm::scale(sandDuneModelMatrix, glm::vec3(5.f, 5.f, 5.f));
	sandDune->RenderModel(shader, sandDuneModelMatrix);

	glm::mat4 anchorModelMatrix = glm::mat4(1.0f);
	anchorModelMatrix = glm::translate(anchorModelMatrix, glm::vec3(20.0f, -14.f, -15.0f));
	anchorModelMatrix = glm::scale(anchorModelMatrix, glm::vec3(0.13f, 0.13f, 0.13f));
	//anchor->RenderModel(shader, anchorModelMatrix);

	glm::mat4 coralModelMatrix = glm::mat4(1.0f);
	coralModelMatrix = glm::translate(coralModelMatrix, glm::vec3(0.0f, 0.f, 0.0f));
	coralModelMatrix = glm::scale(coralModelMatrix, glm::vec3(0.5f, 0.5f, 0.5f));
	//coral->RenderModel(shader, coralModelMatrix);

	glm::mat4 plantModelMatrix = glm::mat4(1.0f);
	plantModelMatrix = glm::translate(plantModelMatrix, glm::vec3(5.0f, -1.f, 8.0f));
	plantModelMatrix = glm::scale(plantModelMatrix, glm::vec3(10.f, 10.f, 10.f));
	plantModelMatrix = glm::rotate(plantModelMatrix, glm::radians(180.0f), glm::vec3(1.0f, 0.0f, 0.0f));
	//plant->RenderModel(shader, plantModelMatrix);



	fishObj->RenderModel(shader, fishModel);
	if (isInFishPerspective)
	{
		//Apply fishAngleY to fish model
		fishModel = glm::rotate(fishModel, glm::radians(fishAngleZ), glm::vec3(0.0f, 0.0f, 1.0f));
		fishModel = glm::rotate(fishModel, glm::radians(fishAngleY), glm::vec3(0.0f, 1.0f, 0.0f));
	}



	glEnable(GL_CULL_FACE);

	for (int i = 0; i < numBubbles; i++)
	{

		//shadowMappingShader.SetFloat("alpha", 0.5f);
		glm::mat4 bubbleModelMatrix = glm::mat4(1.0f);
		updateBubblePosition(i);
		bubbleModelMatrix = glm::translate(bubbleModelMatrix, bubbles[i].position);
		bubbleModelMatrix = glm::scale(bubbleModelMatrix, glm::vec3(bubbles[i].size, bubbles[i].size, bubbles[i].size));
		bubbleObj->RenderModel(shader, bubbleModelMatrix);
	}
	glDisable(GL_CULL_FACE);
	floorObj->RenderMesh(shader);
	glm::mat4 cubeModelMatrix = glm::mat4(1.0f);
	cubeModelMatrix = glm::translate(cubeModelMatrix, glm::vec3(0.0f, 0.f, 0.0f));
	cubeModelMatrix = glm::scale(cubeModelMatrix, glm::vec3(2.f, 1.0f, 2.f));
	cubeObj->RenderMesh(shader, cubeModelMatrix);

}


void KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
	if (key == GLFW_KEY_P && action == GLFW_PRESS)
	{
		timeAcceleration += 0.05f;
	}
}

void FramebufferSizeCallback(GLFWwindow* window, int width, int height)
{
	pCamera->Reshape(width, height);
}

void MouseCallback(GLFWwindow* window, double xpos, double ypos)
{
	pCamera->MouseControl((float)xpos, (float)ypos);
}

void ScrollCallback(GLFWwindow* window, double xoffset, double yOffset)
{
	pCamera->ProcessMouseScroll((float)yOffset);
}

