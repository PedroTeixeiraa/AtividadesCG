/* ============================================================================
 *  JEEP RENEGADE — Visualizador Final 3D
 *  Computacao Grafica / Unisinos
 *
 *  Esta versao deixa tudo em um unico arquivo para facilitar a apresentacao.
 *  A cena integra os requisitos minimos da disciplina:
 *    - Carregamento de mais de um OBJ
 *    - Materiais lidos do .mtl (Ka, Kd, Ks, Ns) e textura
 *    - Iluminacao de Phong com ate 3 fontes de luz
 *    - Camera FPS com teclado e mouse
 *    - Selecao e transformacao de objetos
 *    - Animacao do Jeep por uma trajetoria fechada de Bezier
 *
 *  ONDE ESTA CADA PARTE PARA A ARGUICAO:
 *    - Parser do arquivo de cena .............. loadSceneConfig()
 *    - Parser do material .mtl ................ loadPhongMaterialsFromMTL()
 *    - Parser do OBJ .......................... loadSimpleOBJ()
 *    - Passagem de uniforms ................... loop principal do main()
 *    - Matriz de Model ........................ bloco "Matriz Model" no main()
 *    - Matriz de View ......................... Camera::getViewMatrix()
 *    - Iluminacao de Phong .................... fragmentShaderSource
 *    - Curva de Bezier ........................ cubicBezier() / updateTrajectory()
 * ========================================================================== */

#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

using namespace std;

// GLAD
#include <glad/glad.h>

// GLFW
#include <GLFW/glfw3.h>

// GLM
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

void key_callback(GLFWwindow* window, int key, int scancode, int action, int mode);
void mouse_callback(GLFWwindow* window, double xpos, double ypos);
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);
void processInput(GLFWwindow* window, float deltaTime);
int setupShader();
bool setupScene();

// Material Phong usado pelo Fragment Shader.
// Os valores sao carregados do .mtl quando existem no arquivo.
struct PhongMaterial
{
	// Valores padrao usados caso algum campo nao exista no .mtl.
	glm::vec3 ka = glm::vec3(0.1f, 0.1f, 0.1f);
	glm::vec3 kd = glm::vec3(1.0f, 1.0f, 1.0f);
	glm::vec3 ks = glm::vec3(0.5f, 0.5f, 0.5f);
	float ns = 32.0f;
	string mapKd;
};

// Um OBJ pode ter varios materiais; cada lote usa um material/textura do MTL.
struct MeshMaterialBatch
{
	GLint first = 0;
	GLsizei count = 0;
	PhongMaterial material;
	GLuint textureID = 0;
	bool useTexture = false;
};

// Malha carregada uma vez; os objetos da cena apenas apontam para ela.
struct MeshResource
{
	string name;
	GLuint vao = 0;
	GLsizei vertexCount = 0;
	vector<MeshMaterialBatch> batches;
};

enum CameraMovement
{
	CAMERA_FORWARD = 0,
	CAMERA_BACKWARD = 1,
	CAMERA_LEFT = 2,
	CAMERA_RIGHT = 3
};

class Camera
{
public:
	Camera(const glm::vec3& startPosition = glm::vec3(0.0f, 2.4f, 8.5f), const glm::vec3& worldUp = glm::vec3(0.0f, 1.0f, 0.0f), float startYaw = -90.0f, float startPitch = -12.0f)
		: position(startPosition), worldUp(worldUp), yaw(startYaw), pitch(startPitch), movementSpeed(4.0f), mouseSensitivity(0.08f), fov(60.0f)
	{
		front = glm::vec3(0.0f, 0.0f, -1.0f);
		updateCameraVectors();
	}

	glm::mat4 getViewMatrix() const
	{
		// Matriz View: transforma o mundo para o ponto de vista da camera.
		return glm::lookAt(position, position + front, up);
	}

	const glm::vec3& getPosition() const
	{
		return position;
	}

	float getFov() const
	{
		return fov;
	}

	void setPosition(const glm::vec3& newPosition)
	{
		position = newPosition;
	}

	void move(CameraMovement direction, float deltaTime)
	{
		const float velocity = movementSpeed * deltaTime;
		if (direction == CAMERA_FORWARD)
		{
			position += front * velocity;
		}
		else if (direction == CAMERA_BACKWARD)
		{
			position -= front * velocity;
		}
		else if (direction == CAMERA_LEFT)
		{
			position -= right * velocity;
		}
		else if (direction == CAMERA_RIGHT)
		{
			position += right * velocity;
		}
	}

	void rotate(float xOffset, float yOffset)
	{
		yaw += xOffset * mouseSensitivity;
		pitch += yOffset * mouseSensitivity;
		pitch = glm::clamp(pitch, -89.0f, 89.0f);
		updateCameraVectors();
	}

	void zoom(float yOffset)
	{
		fov -= yOffset;
		fov = glm::clamp(fov, 1.0f, 60.0f);
	}

private:
	void updateCameraVectors()
	{
		// Converte yaw/pitch em vetores frente/direita/cima da camera FPS.
		glm::vec3 frontDirection;
		frontDirection.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
		frontDirection.y = sin(glm::radians(pitch));
		frontDirection.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
		front = glm::normalize(frontDirection);
		right = glm::normalize(glm::cross(front, worldUp));
		up = glm::normalize(glm::cross(right, front));
	}

	glm::vec3 position;
	glm::vec3 front;
	glm::vec3 up;
	glm::vec3 right;
	glm::vec3 worldUp;
	float yaw;
	float pitch;
	float movementSpeed;
	float mouseSensitivity;
	float fov;
};

bool loadPhongMaterialsFromMTL(const string& mtlFilePath, unordered_map<string, PhongMaterial>& outMaterials, vector<string>& outOrder);
int loadSimpleOBJ(const string& filePath, int& nVertices, string& outTexturePath, bool& outHasTexCoords, PhongMaterial& outMaterial);
string trim(const string& value);
GLuint loadTexture(const string& filePath, int& width, int& height);

const GLuint WIDTH = 1000, HEIGHT = 1000;
const float MOVE_STEP = 0.1f;
const float SCALE_STEP = 0.1f;
const float MIN_SCALE = 0.2f;
const float TRAJECTORY_SPEED = 1.0f;
vector<MeshMaterialBatch> gMeshBatches;
vector<GLuint> gOwnedTextureIDs;
GLuint gFallbackTextureID = 0;

const float PHONG_AMBIENT_GAIN = 0.45f;
const float PHONG_DIFFUSE_GAIN = 1.25f;
const float PHONG_SPECULAR_GAIN = 2.8f;

// Vertex Shader:
// recebe posicao, UV, normal e cor; aplica Model, View e Projection.
const GLchar* vertexShaderSource = "#version 410\n"
"layout (location = 0) in vec3 position;\n"
"layout (location = 1) in vec2 texCoord;\n"
"layout (location = 2) in vec3 normal;\n"
"layout (location = 3) in vec3 color;\n"
"uniform mat4 model;\n"
"uniform mat4 view;\n"
"uniform mat4 projection;\n"
"out vec2 finalTexCoord;\n"
"out vec3 finalNormal;\n"
"out vec3 fragPos;\n"
"out vec3 finalColor;\n"
"void main()\n"
"{\n"
"vec4 worldPos = model * vec4(position, 1.0);\n"
"gl_Position = projection * view * worldPos;\n"
"finalTexCoord = texCoord;\n"
"fragPos = vec3(worldPos);\n"
"mat3 normalMatrix = transpose(inverse(mat3(model)));\n"
"finalNormal = normalize(normalMatrix * normal);\n"
"finalColor = color;\n"
"}\0";

// Fragment Shader:
// aqui fica o calculo de iluminacao de Phong usando Ka, Kd, Ks e Ns do .mtl.
const GLchar* fragmentShaderSource = "#version 410\n"
"in vec2 finalTexCoord;\n"
"in vec3 finalNormal;\n"
"in vec3 fragPos;\n"
"in vec3 finalColor;\n"
"uniform sampler2D texBuff;\n"
"uniform bool useTexture;\n"
"uniform bool showTexture;\n"
"uniform bool showMaterialOnly;\n"
"uniform bool useMaterialCoefficients;\n"
"uniform vec3 lightPositions[3];\n"
"uniform vec3 lightColors[3];\n"
"uniform int lightEnabled[3];\n"
"uniform float lightIntensity[3];\n"
"uniform vec3 camPos;\n"
"uniform vec3 matKa;\n"
"uniform vec3 matKd;\n"
"uniform vec3 matKs;\n"
"uniform float matNs;\n"
"uniform float ambientGain;\n"
"uniform float diffuseGain;\n"
"uniform float specularGain;\n"
"out vec4 color;\n"
"void main()\n"
"{\n"
"vec3 materialDiffuse = useMaterialCoefficients ? matKd : vec3(1.0);\n"
"vec3 materialAmbient = useMaterialCoefficients ? matKa : vec3(0.15);\n"
"vec3 materialSpecular = useMaterialCoefficients ? matKs : vec3(0.4);\n"
"vec3 baseColor = (useTexture && showTexture) ? texture(texBuff, finalTexCoord).rgb : finalColor;\n"
"if (showMaterialOnly) { baseColor = matKd; }\n"
"vec3 N = normalize(finalNormal);\n"
"vec3 V = normalize(camPos - fragPos);\n"
"vec3 result = vec3(0.0);\n"
"for (int i = 0; i < 3; ++i)\n"
"{\n"
"if (lightEnabled[i] == 0) { continue; }\n"
"vec3 L = normalize(lightPositions[i] - fragPos);\n"
"vec3 lightColor = lightColors[i] * lightIntensity[i];\n"
"vec3 ambient = ambientGain * materialAmbient * lightColor;\n"
"float diff = max(dot(N, L), 0.0);\n"
"vec3 diffuse = diffuseGain * materialDiffuse * diff * lightColor;\n"
"vec3 specular = vec3(0.0);\n"
"if (diff > 0.0) {\n"
"vec3 R = reflect(-L, N);\n"
"float spec = pow(max(dot(V, R), 0.0), max(matNs * 1.5, 1.0));\n"
"specular = specularGain * materialSpecular * spec * lightColor;\n"
"}\n"
"result += (ambient + diffuse) * baseColor + specular;\n"
"}\n"
"if (length(result) < 0.0001) { result = ambientGain * materialAmbient * baseColor; }\n"
"color = vec4(result, 1.0);\n"
"}\n\0";

enum RotationAxis
{
	AXIS_NONE = 0,
	AXIS_X = 1,
	AXIS_Y = 2,
	AXIS_Z = 3
};

struct SceneObject
{
	string name;
	// Indice para gMeshes: permite instanciar uma malha sem recarregar o OBJ.
	size_t meshIndex;
	glm::vec3 position;
	glm::vec3 rotationDegrees;
	float scale;
	RotationAxis rotationAxis;
	vector<glm::vec3> controlPoints;
	size_t nextControlPointIndex;
	float trajectorySpeed;
	float trajectoryT;
	bool animationEnabled;
};

// Luz simples usada pelo shader Phong.
struct SceneLight
{
	glm::vec3 position;
	glm::vec3 color;
	float intensity;
	bool enabled;
};

vector<MeshResource> gMeshes;
vector<SceneObject> gObjects;
vector<SceneLight> gLights;
Camera gCamera;
size_t activeObjectIndex = 0;

// Flags alteradas por teclado para demonstrar recursos durante a apresentacao.
bool gAnimateTrajectories = false;
bool gShowTexture = true;
bool gShowMaterialOnly = false;
bool gUseMaterialCoefficients = true;
bool gFirstMouse = true;
float gLastX = static_cast<float>(WIDTH) / 2.0f;
float gLastY = static_cast<float>(HEIGHT) / 2.0f;

SceneObject& activeObject()
{
	return gObjects[activeObjectIndex];
}

void addSceneObject(size_t meshIndex, const string& name, const glm::vec3& position, const glm::vec3& rotationDegrees, float scale, const vector<glm::vec3>& trajectory)
{
	SceneObject instance;
	instance.name = name;
	instance.meshIndex = meshIndex;
	instance.position = position;
	instance.rotationDegrees = rotationDegrees;
	instance.scale = scale;
	instance.rotationAxis = AXIS_NONE;
	instance.controlPoints = trajectory;
	instance.nextControlPointIndex = 0;
	instance.trajectorySpeed = TRAJECTORY_SPEED;
	instance.trajectoryT = 0.0f;
	instance.animationEnabled = !trajectory.empty();
	gObjects.push_back(instance);
	activeObjectIndex = gObjects.size() - 1;
}

void cycleActiveObject()
{
	if (gObjects.empty())
	{
		return;
	}
	activeObjectIndex = (activeObjectIndex + 1) % gObjects.size();
	cout << "Objeto ativo: " << activeObjectIndex << " (" << activeObject().name << ")" << endl;
}

void moveActiveObject(const glm::vec3& delta)
{
	if (gObjects.empty())
	{
		return;
	}
	activeObject().position += delta;
}

glm::vec3 cubicBezier(const glm::vec3& p0, const glm::vec3& p1, const glm::vec3& p2, const glm::vec3& p3, float t)
{
	// Curva de Bezier cubica: combina 4 pontos de controle pelo parametro t.
	const float u = 1.0f - t;
	return (u * u * u) * p0
		+ (3.0f * u * u * t) * p1
		+ (3.0f * u * t * t) * p2
		+ (t * t * t) * p3;
}

void updateTrajectory(SceneObject& object, float deltaTime)
{
	if (!gAnimateTrajectories || !object.animationEnabled || object.controlPoints.size() < 4 || deltaTime <= 0.0f)
	{
		return;
	}

	object.trajectoryT += object.trajectorySpeed * 0.25f * deltaTime;
	while (object.trajectoryT >= 1.0f)
	{
		object.trajectoryT -= 1.0f;
		object.nextControlPointIndex = (object.nextControlPointIndex + 1) % object.controlPoints.size();
	}

	const size_t count = object.controlPoints.size();
	const size_t i0 = object.nextControlPointIndex % count;
	const size_t i1 = (i0 + 1) % count;
	const size_t i2 = (i0 + 2) % count;
	const size_t i3 = (i0 + 3) % count;
	// A posicao do objeto vem diretamente do ponto calculado na curva.
	object.position = cubicBezier(object.controlPoints[i0], object.controlPoints[i1], object.controlPoints[i2], object.controlPoints[i3], object.trajectoryT);
}

void scaleActiveObject(float delta)
{
	if (gObjects.empty())
	{
		return;
	}
	activeObject().scale += delta;
	if (activeObject().scale < MIN_SCALE)
	{
		activeObject().scale = MIN_SCALE;
	}
}

void setActiveRotationAxis(RotationAxis axis)
{
	if (gObjects.empty())
	{
		return;
	}
	activeObject().rotationAxis = axis;
}

void printControls()
{
	cout << "Camera" << endl;
	cout << "  W, A, S, D + mouse: navegar pela cena" << endl;
	cout << "  Scroll: zoom / campo de visao" << endl;
	cout << endl;
	cout << "Objeto selecionado" << endl;
	cout << "  TAB: alternar objeto ativo" << endl;
	cout << "  Setas: mover no plano XZ" << endl;
	cout << "  I, J: mover no eixo Y" << endl;
	cout << "  X, Y, Z: ativar rotacao automatica no eixo escolhido" << endl;
	cout << "  [ e ]: diminuir/aumentar escala uniforme" << endl;
	cout << endl;
	cout << "Materiais, texturas e luz" << endl;
	cout << "  T: ligar/desligar textura" << endl;
	cout << "  M: mostrar Kd do material sem textura" << endl;
	cout << "  K: ligar/desligar uso de Ka/Kd/Ks do MTL no shader" << endl;
	cout << "  1, 2, 3: ligar/desligar cada luz da iluminacao de 3 pontos" << endl;
	cout << endl;
	cout << "Animacao" << endl;
	cout << "  P: iniciar/pausar trajetoria Bezier" << endl;
	cout << endl;
	cout << endl;
	cout << "ESC: fecha a janela" << endl;
	cout << "============================================================" << endl;
	cout << endl;
}

int main()
{
	glfwInit();

	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

#ifdef __APPLE__
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

	GLFWwindow* window = glfwCreateWindow(WIDTH, HEIGHT, "Jeep Renegade -- Visualizador 3D", nullptr, nullptr);
	if (!window)
	{
		cout << "Falha ao criar janela GLFW" << endl;
		glfwTerminate();
		return -1;
	}
	glfwMakeContextCurrent(window);
	glfwSetKeyCallback(window, key_callback);
	glfwSetCursorPosCallback(window, mouse_callback);
	glfwSetScrollCallback(window, scroll_callback);
	glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

	if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
	{
		std::cout << "Failed to initialize GLAD" << std::endl;
	}

	const GLubyte* renderer = glGetString(GL_RENDERER);
	const GLubyte* version = glGetString(GL_VERSION);
	cout << "Renderer: " << renderer << endl;
	cout << "OpenGL version supported " << version << endl;

	int width, height;
	glfwGetFramebufferSize(window, &width, &height);
	glViewport(0, 0, width, height);

	const unsigned char whitePixel[] = { 255, 255, 255, 255 };
	// Textura branca de seguranca para materiais sem imagem.
	glGenTextures(1, &gFallbackTextureID);
	glBindTexture(GL_TEXTURE_2D, gFallbackTextureID);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, whitePixel);
	glBindTexture(GL_TEXTURE_2D, 0);

	GLuint shaderID = setupShader();
	if (!setupScene())
	{
		cerr << "Falha ao preparar a cena final." << endl;
		glfwTerminate();
		return -1;
	}

	glUseProgram(shaderID);

	// Localizacoes dos uniforms enviados do C++ para os shaders.
	GLint modelLoc = glGetUniformLocation(shaderID, "model");
	GLint viewLoc = glGetUniformLocation(shaderID, "view");
	GLint projectionLoc = glGetUniformLocation(shaderID, "projection");
	GLint texBuffLoc = glGetUniformLocation(shaderID, "texBuff");
	GLint useTextureLoc = glGetUniformLocation(shaderID, "useTexture");
	GLint showTextureLoc = glGetUniformLocation(shaderID, "showTexture");
	GLint showMaterialOnlyLoc = glGetUniformLocation(shaderID, "showMaterialOnly");
	GLint useMaterialCoefficientsLoc = glGetUniformLocation(shaderID, "useMaterialCoefficients");
	GLint lightPositionsLoc = glGetUniformLocation(shaderID, "lightPositions[0]");
	GLint lightColorsLoc = glGetUniformLocation(shaderID, "lightColors[0]");
	GLint lightEnabledLoc = glGetUniformLocation(shaderID, "lightEnabled[0]");
	GLint lightIntensityLoc = glGetUniformLocation(shaderID, "lightIntensity[0]");
	GLint camPosLoc = glGetUniformLocation(shaderID, "camPos");
	GLint matKaLoc = glGetUniformLocation(shaderID, "matKa");
	GLint matKdLoc = glGetUniformLocation(shaderID, "matKd");
	GLint matKsLoc = glGetUniformLocation(shaderID, "matKs");
	GLint matNsLoc = glGetUniformLocation(shaderID, "matNs");
	GLint ambientGainLoc = glGetUniformLocation(shaderID, "ambientGain");
	GLint diffuseGainLoc = glGetUniformLocation(shaderID, "diffuseGain");
	GLint specularGainLoc = glGetUniformLocation(shaderID, "specularGain");

	glUniform1i(texBuffLoc, 0);
	glUniform1f(ambientGainLoc, PHONG_AMBIENT_GAIN);
	glUniform1f(diffuseGainLoc, PHONG_DIFFUSE_GAIN);
	glUniform1f(specularGainLoc, PHONG_SPECULAR_GAIN);

	glEnable(GL_DEPTH_TEST);

	printControls();
	float previousTime = static_cast<float>(glfwGetTime());

	while (!glfwWindowShouldClose(window))
	{
		glfwPollEvents();
		const float currentTime = static_cast<float>(glfwGetTime());
		const float deltaTime = currentTime - previousTime;
		previousTime = currentTime;

		// Entrada continua: camera FPS e animacao por tempo.
		processInput(window, deltaTime);

		for (SceneObject& object : gObjects)
		{
			updateTrajectory(object, deltaTime);
		}

		glfwGetFramebufferSize(window, &width, &height);
		glViewport(0, 0, width, height);
		glClearColor(0.10f, 0.10f, 0.12f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		glLineWidth(10);
		glPointSize(8);

		const float angle = currentTime;

		// Atualiza View/Projection a cada frame para suportar camera e resize.
		const glm::mat4 view = gCamera.getViewMatrix();
		const float aspectRatio = (height > 0) ? (static_cast<float>(width) / static_cast<float>(height)) : 1.0f;
		const glm::mat4 projection = glm::perspective(glm::radians(gCamera.getFov()), aspectRatio, 0.1f, 100.0f);
		glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(view));
		glUniformMatrix4fv(projectionLoc, 1, GL_FALSE, glm::value_ptr(projection));
		glUniform3fv(camPosLoc, 1, glm::value_ptr(gCamera.getPosition()));
		glUniform1i(showTextureLoc, gShowTexture ? 1 : 0);
		glUniform1i(showMaterialOnlyLoc, gShowMaterialOnly ? 1 : 0);
		glUniform1i(useMaterialCoefficientsLoc, gUseMaterialCoefficients ? 1 : 0);

		// Uniforms das luzes: posicao, cor, intensidade e ligado/desligado.
		// O Fragment Shader soma essas contribuicoes no modelo de Phong.
		glm::vec3 lightPositions[3] = { glm::vec3(0.0f), glm::vec3(0.0f), glm::vec3(0.0f) };
		glm::vec3 lightColors[3] = { glm::vec3(1.0f), glm::vec3(1.0f), glm::vec3(1.0f) };
		GLint lightEnabled[3] = { 0, 0, 0 };
		GLfloat lightIntensity[3] = { 0.0f, 0.0f, 0.0f };
		for (size_t i = 0; i < gLights.size() && i < 3; ++i)
		{
			lightPositions[i] = gLights[i].position;
			lightColors[i] = gLights[i].color;
			lightEnabled[i] = gLights[i].enabled ? 1 : 0;
			lightIntensity[i] = gLights[i].intensity;
		}
		glUniform3fv(lightPositionsLoc, 3, glm::value_ptr(lightPositions[0]));
		glUniform3fv(lightColorsLoc, 3, glm::value_ptr(lightColors[0]));
		glUniform1iv(lightEnabledLoc, 3, lightEnabled);
		glUniform1fv(lightIntensityLoc, 3, lightIntensity);

		glActiveTexture(GL_TEXTURE0);
		for (const SceneObject& object : gObjects)
		{
			if (object.meshIndex >= gMeshes.size())
			{
				continue;
			}
			const MeshResource& mesh = gMeshes[object.meshIndex];
			// Matriz Model:
			// transforma os vertices do espaco local do OBJ para o mundo.
			// Ordem usada: Translacao * Rotacao * Escala.
			glm::mat4 model = glm::mat4(1.0f);
			model = glm::translate(model, object.position);
			model = glm::rotate(model, glm::radians(object.rotationDegrees.x), glm::vec3(1.0f, 0.0f, 0.0f));
			model = glm::rotate(model, glm::radians(object.rotationDegrees.y), glm::vec3(0.0f, 1.0f, 0.0f));
			model = glm::rotate(model, glm::radians(object.rotationDegrees.z), glm::vec3(0.0f, 0.0f, 1.0f));

			if (object.rotationAxis == AXIS_X)
			{
				model = glm::rotate(model, angle, glm::vec3(1.0f, 0.0f, 0.0f));
			}
			else if (object.rotationAxis == AXIS_Y)
			{
				model = glm::rotate(model, angle, glm::vec3(0.0f, 1.0f, 0.0f));
			}
			else if (object.rotationAxis == AXIS_Z)
			{
				model = glm::rotate(model, angle, glm::vec3(0.0f, 0.0f, 1.0f));
			}

			model = glm::scale(model, glm::vec3(object.scale));

			glBindVertexArray(mesh.vao);
			glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));
			for (const MeshMaterialBatch& batch : mesh.batches)
			{
				// Uniforms do material:
				// cada lote do OBJ pode usar um material diferente do .mtl.
				glUniform3fv(matKaLoc, 1, glm::value_ptr(batch.material.ka));
				glUniform3fv(matKdLoc, 1, glm::value_ptr(batch.material.kd));
				glUniform3fv(matKsLoc, 1, glm::value_ptr(batch.material.ks));
				glUniform1f(matNsLoc, batch.material.ns);
				glUniform1i(useTextureLoc, batch.useTexture ? 1 : 0);
				glBindTexture(GL_TEXTURE_2D, batch.useTexture ? batch.textureID : gFallbackTextureID);
				glDrawArrays(GL_TRIANGLES, batch.first, batch.count);
			}
		}
		glBindVertexArray(0);

		glfwSwapBuffers(window);
	}

	for (GLuint textureID : gOwnedTextureIDs)
	{
		if (textureID != 0)
		{
			glDeleteTextures(1, &textureID);
		}
	}
	for (const MeshResource& mesh : gMeshes)
	{
		if (mesh.vao != 0)
		{
			glDeleteVertexArrays(1, &mesh.vao);
		}
	}
	if (gFallbackTextureID != 0)
	{
		glDeleteTextures(1, &gFallbackTextureID);
	}
	glfwTerminate();
	return 0;
}

void key_callback(GLFWwindow* window, int key, int scancode, int action, int mode)
{
	(void)scancode;
	(void)mode;

	if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
	{
		glfwSetWindowShouldClose(window, GL_TRUE);
		return;
	}

	if (!(action == GLFW_PRESS || action == GLFW_REPEAT))
	{
		return;
	}

	// Controles discretos: selecao, transformacao e toggles de demonstracao.
	if (key == GLFW_KEY_X)
	{
		setActiveRotationAxis(AXIS_X);
	}
	else if (key == GLFW_KEY_Y)
	{
		setActiveRotationAxis(AXIS_Y);
	}
	else if (key == GLFW_KEY_Z)
	{
		setActiveRotationAxis(AXIS_Z);
	}
	else if (key == GLFW_KEY_LEFT)
	{
		moveActiveObject(glm::vec3(-MOVE_STEP, 0.0f, 0.0f));
	}
	else if (key == GLFW_KEY_RIGHT)
	{
		moveActiveObject(glm::vec3(MOVE_STEP, 0.0f, 0.0f));
	}
	else if (key == GLFW_KEY_UP)
	{
		moveActiveObject(glm::vec3(0.0f, 0.0f, -MOVE_STEP));
	}
	else if (key == GLFW_KEY_DOWN)
	{
		moveActiveObject(glm::vec3(0.0f, 0.0f, MOVE_STEP));
	}
	else if (key == GLFW_KEY_I)
	{
		moveActiveObject(glm::vec3(0.0f, MOVE_STEP, 0.0f));
	}
	else if (key == GLFW_KEY_J)
	{
		moveActiveObject(glm::vec3(0.0f, -MOVE_STEP, 0.0f));
	}
	else if (key == GLFW_KEY_LEFT_BRACKET)
	{
		scaleActiveObject(-SCALE_STEP);
	}
	else if (key == GLFW_KEY_RIGHT_BRACKET)
	{
		scaleActiveObject(SCALE_STEP);
	}
	else if (key == GLFW_KEY_TAB && action == GLFW_PRESS)
	{
		cycleActiveObject();
	}
	else if (key == GLFW_KEY_1 && action == GLFW_PRESS && gLights.size() > 0)
	{
		gLights[0].enabled = !gLights[0].enabled;
		cout << "Luz 1: " << (gLights[0].enabled ? "ligada" : "desligada") << endl;
	}
	else if (key == GLFW_KEY_2 && action == GLFW_PRESS && gLights.size() > 1)
	{
		gLights[1].enabled = !gLights[1].enabled;
		cout << "Luz 2: " << (gLights[1].enabled ? "ligada" : "desligada") << endl;
	}
	else if (key == GLFW_KEY_3 && action == GLFW_PRESS && gLights.size() > 2)
	{
		gLights[2].enabled = !gLights[2].enabled;
		cout << "Luz 3: " << (gLights[2].enabled ? "ligada" : "desligada") << endl;
	}
	else if (key == GLFW_KEY_T && action == GLFW_PRESS)
	{
		gShowTexture = !gShowTexture;
		cout << "Textura: " << (gShowTexture ? "ligada" : "desligada") << endl;
	}
	else if (key == GLFW_KEY_M && action == GLFW_PRESS)
	{
		gShowMaterialOnly = !gShowMaterialOnly;
		cout << "Visualizacao Kd: " << (gShowMaterialOnly ? "ligada" : "desligada") << endl;
	}
	else if (key == GLFW_KEY_K && action == GLFW_PRESS)
	{
		gUseMaterialCoefficients = !gUseMaterialCoefficients;
		cout << "Coeficientes Ka/Kd/Ks: " << (gUseMaterialCoefficients ? "ligados" : "desligados") << endl;
	}
	else if (key == GLFW_KEY_P && action == GLFW_PRESS)
	{
		gAnimateTrajectories = !gAnimateTrajectories;
		cout << "Animacao Bezier: " << (gAnimateTrajectories ? "rodando" : "pausada") << endl;
	}
}

void processInput(GLFWwindow* window, float deltaTime)
{
	// Movimento continuo da camera; usa deltaTime para nao depender do FPS.
	if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
	{
		gCamera.move(CAMERA_FORWARD, deltaTime);
	}
	if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
	{
		gCamera.move(CAMERA_BACKWARD, deltaTime);
	}
	if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
	{
		gCamera.move(CAMERA_LEFT, deltaTime);
	}
	if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
	{
		gCamera.move(CAMERA_RIGHT, deltaTime);
	}
}

void mouse_callback(GLFWwindow* window, double xpos, double ypos)
{
	(void)window;
	if (gFirstMouse)
	{
		gLastX = static_cast<float>(xpos);
		gLastY = static_cast<float>(ypos);
		gFirstMouse = false;
	}

	// Offset do mouse controla yaw/pitch da camera.
	const float xOffset = static_cast<float>(xpos) - gLastX;
	const float yOffset = gLastY - static_cast<float>(ypos);
	gLastX = static_cast<float>(xpos);
	gLastY = static_cast<float>(ypos);
	gCamera.rotate(xOffset, yOffset);
}

void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
	(void)window;
	(void)xoffset;
	gCamera.zoom(static_cast<float>(yoffset));
}

int setupShader()
{
	GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
	glShaderSource(vertexShader, 1, &vertexShaderSource, NULL);
	glCompileShader(vertexShader);

	GLint success;
	GLchar infoLog[512];
	glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);
	if (!success)
	{
		glGetShaderInfoLog(vertexShader, 512, NULL, infoLog);
		std::cout << "ERROR::SHADER::VERTEX::COMPILATION_FAILED\n" << infoLog << std::endl;
	}

	GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(fragmentShader, 1, &fragmentShaderSource, NULL);
	glCompileShader(fragmentShader);

	glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);
	if (!success)
	{
		glGetShaderInfoLog(fragmentShader, 512, NULL, infoLog);
		std::cout << "ERROR::SHADER::FRAGMENT::COMPILATION_FAILED\n" << infoLog << std::endl;
	}

	GLuint shaderProgram = glCreateProgram();
	glAttachShader(shaderProgram, vertexShader);
	glAttachShader(shaderProgram, fragmentShader);
	glLinkProgram(shaderProgram);

	glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success);
	if (!success)
	{
		glGetProgramInfoLog(shaderProgram, 512, NULL, infoLog);
		std::cout << "ERROR::SHADER::PROGRAM::LINKING_FAILED\n" << infoLog << std::endl;
	}
	glDeleteShader(vertexShader);
	glDeleteShader(fragmentShader);

	return shaderProgram;
}

string trim(const string& value)
{
	const size_t begin = value.find_first_not_of(" \t\r\n");
	if (begin == string::npos)
	{
		return "";
	}
	const size_t end = value.find_last_not_of(" \t\r\n");
	return value.substr(begin, end - begin + 1);
}

bool loadPhongMaterialsFromMTL(const string& mtlFilePath, unordered_map<string, PhongMaterial>& outMaterials, vector<string>& outOrder)
{
	// Parser simples de MTL: le Ka, Kd, Ks, Ns e map_Kd de cada material.
	ifstream mtlFile(mtlFilePath.c_str());
	if (!mtlFile.is_open())
	{
		return false;
	}

	struct MaterialRecord
	{
		string name;
		PhongMaterial material;
	};

	vector<MaterialRecord> materials;
	MaterialRecord current;
	bool hasCurrent = false;

	auto flushCurrent = [&]()
	{
		if (hasCurrent && !current.name.empty())
		{
			materials.push_back(current);
		}
	};

	string line;
	while (getline(mtlFile, line))
	{
		line = trim(line);
		if (line.empty() || line[0] == '#')
		{
			continue;
		}

		istringstream ss(line);
		string word;
		ss >> word;
		if (word == "newmtl")
		{
			flushCurrent();
			current = MaterialRecord();
			string rest;
			getline(ss, rest);
			current.name = trim(rest);
			hasCurrent = true;
		}
		else if (word == "Ka" && hasCurrent)
		{
			ss >> current.material.ka.x >> current.material.ka.y >> current.material.ka.z;
		}
		else if (word == "Kd" && hasCurrent)
		{
			ss >> current.material.kd.x >> current.material.kd.y >> current.material.kd.z;
		}
		else if (word == "Ks" && hasCurrent)
		{
			ss >> current.material.ks.x >> current.material.ks.y >> current.material.ks.z;
		}
		else if (word == "Ns" && hasCurrent)
		{
			ss >> current.material.ns;
		}
		else if (word == "map_Kd" && hasCurrent)
		{
			getline(ss, current.material.mapKd);
			current.material.mapKd = trim(current.material.mapKd);
		}
	}

	flushCurrent();
	if (materials.empty())
	{
		return false;
	}

	outMaterials.clear();
	outOrder.clear();
	for (const MaterialRecord& record : materials)
	{
		if (record.name.empty())
		{
			continue;
		}
		outMaterials[record.name] = record.material;
		outOrder.push_back(record.name);
	}
	return !outOrder.empty();
}

GLuint loadTexture(const string& filePath, int& width, int& height)
{
	// stb_image le PNG/JPG e gera uma textura OpenGL 2D.
	GLuint texID = 0;
	glGenTextures(1, &texID);
	glBindTexture(GL_TEXTURE_2D, texID);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	stbi_set_flip_vertically_on_load(true);
	int channels = 0;
	unsigned char* data = stbi_load(filePath.c_str(), &width, &height, &channels, 0);
	if (data == nullptr)
	{
		cerr << "Falha ao carregar textura: " << filePath << endl;
		glBindTexture(GL_TEXTURE_2D, 0);
		glDeleteTextures(1, &texID);
		return 0;
	}

	GLenum format = GL_RGB;
	GLenum internalFormat = GL_RGB8;
	if (channels == 1)
	{
		format = GL_RED;
		internalFormat = GL_R8;
	}
	else if (channels == 4)
	{
		format = GL_RGBA;
		internalFormat = GL_RGBA8;
	}

	glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, width, height, 0, format, GL_UNSIGNED_BYTE, data);
	glGenerateMipmap(GL_TEXTURE_2D);

	stbi_image_free(data);
	glBindTexture(GL_TEXTURE_2D, 0);
	return texID;
}

int loadSimpleOBJ(const string& filePath, int& nVertices, string& outTexturePath, bool& outHasTexCoords, PhongMaterial& outMaterial)
{
	// Loader OBJ: le vertices, UVs, normais, faces, mtllib e usemtl.
	struct OBJIndex
	{
		int vertex = -1;
		int uv = -1;
		int normal = -1;
	};
	struct TriRecord
	{
		array<OBJIndex, 3> indices;
		string materialName;
	};

	vector<glm::vec3> vertices;
	vector<glm::vec2> texCoords;
	vector<glm::vec3> normals;
	vector<TriRecord> triangles;
	vector<GLfloat> vBuffer;
	bool hasBounds = false;
	glm::vec3 minPos(0.0f);
	glm::vec3 maxPos(0.0f);

	string mtlFileName;
	string currentMaterial;
	string firstMaterialUsed;
	outTexturePath.clear();
	outHasTexCoords = false;
	outMaterial = PhongMaterial();
	gMeshBatches.clear();

	ifstream arqEntrada(filePath.c_str());
	if (!arqEntrada.is_open())
	{
		cerr << "Erro ao tentar ler o arquivo " << filePath << endl;
		return -1;
	}

	string line;
	while (getline(arqEntrada, line))
	{
		istringstream ssline(line);
		string word;
		ssline >> word;
		if (word.empty() || word[0] == '#')
		{
			continue;
		}

		if (word == "v")
		{
			glm::vec3 vertice;
			ssline >> vertice.x >> vertice.y >> vertice.z;
			vertices.push_back(vertice);
			if (!hasBounds)
			{
				minPos = vertice;
				maxPos = vertice;
				hasBounds = true;
			}
			else
			{
				minPos.x = min(minPos.x, vertice.x);
				minPos.y = min(minPos.y, vertice.y);
				minPos.z = min(minPos.z, vertice.z);
				maxPos.x = max(maxPos.x, vertice.x);
				maxPos.y = max(maxPos.y, vertice.y);
				maxPos.z = max(maxPos.z, vertice.z);
			}
		}
		else if (word == "vt")
		{
			glm::vec2 vt;
			ssline >> vt.x >> vt.y;
			texCoords.push_back(vt);
		}
		else if (word == "vn")
		{
			glm::vec3 vn;
			ssline >> vn.x >> vn.y >> vn.z;
			normals.push_back(vn);
		}
		else if (word == "mtllib")
		{
			string rest;
			getline(ssline, rest);
			mtlFileName = trim(rest);
		}
		else if (word == "usemtl")
		{
			string rest;
			getline(ssline, rest);
			currentMaterial = trim(rest);
			if (firstMaterialUsed.empty() && !currentMaterial.empty())
			{
				firstMaterialUsed = currentMaterial;
			}
		}
		else if (word == "f")
		{
			vector<OBJIndex> face;
			string token;
			while (ssline >> token)
			{
				OBJIndex idx;
				istringstream sstoken(token);
				string part;

				if (getline(sstoken, part, '/') && !part.empty())
				{
					idx.vertex = stoi(part) - 1;
				}
				if (getline(sstoken, part, '/') && !part.empty())
				{
					idx.uv = stoi(part) - 1;
				}
				if (getline(sstoken, part) && !part.empty())
				{
					idx.normal = stoi(part) - 1;
				}
				face.push_back(idx);
			}

			if (face.size() < 3)
			{
				continue;
			}

			for (size_t i = 1; i + 1 < face.size(); ++i)
			{
				// Se vier uma face com mais de 3 vertices, converte para triangulos.
				triangles.push_back({ { face[0], face[i], face[i + 1] }, currentMaterial });
			}
		}
	}

	if (vertices.empty() || triangles.empty())
	{
		cerr << "OBJ sem vertices renderizaveis: " << filePath << endl;
		return -1;
	}

	glm::vec3 modelCenter(0.0f, 0.0f, 0.0f);
	float modelScale = 1.0f;
	if (hasBounds)
	{
		modelCenter = (minPos + maxPos) * 0.5f;
		const glm::vec3 size = maxPos - minPos;
		const float maxExtent = max(size.x, max(size.y, size.z));
		if (maxExtent > 1e-6f)
		{
			modelScale = 2.0f / maxExtent;
		}
	}

	unordered_map<string, PhongMaterial> materialByName;
	vector<string> materialOrder;
	filesystem::path selectedMtlPath;
	filesystem::path objPath(filePath);
	bool materialLoaded = false;
	if (!mtlFileName.empty())
	{
		filesystem::path mtlPath = objPath.parent_path() / mtlFileName;
		materialLoaded = loadPhongMaterialsFromMTL(mtlPath.string(), materialByName, materialOrder);
		if (materialLoaded)
		{
			selectedMtlPath = mtlPath;
		}
	}
	if (!materialLoaded)
	{
		filesystem::path fallbackMtlPath = objPath.parent_path() / (objPath.stem().string() + ".mtl");
		materialLoaded = loadPhongMaterialsFromMTL(fallbackMtlPath.string(), materialByName, materialOrder);
		if (materialLoaded)
		{
			selectedMtlPath = fallbackMtlPath;
		}
	}
	if (!materialLoaded)
	{
		cerr << "Falha ao localizar MTL para o OBJ: " << filePath << endl;
	}

	auto resolveMaterial = [&](const string& name) -> PhongMaterial
	{
		if (!name.empty())
		{
			auto it = materialByName.find(name);
			if (it != materialByName.end())
			{
				return it->second;
			}
		}
		if (!firstMaterialUsed.empty())
		{
			auto it = materialByName.find(firstMaterialUsed);
			if (it != materialByName.end())
			{
				return it->second;
			}
		}
		if (!materialOrder.empty())
		{
			return materialByName[materialOrder.front()];
		}
		return PhongMaterial();
	};

	unordered_map<string, GLuint> textureCache;
	auto resolveTextureID = [&](const PhongMaterial& material) -> GLuint
	{
		if (!materialLoaded || material.mapKd.empty())
		{
			return 0;
		}
		string normalizedMap = material.mapKd;
		replace(normalizedMap.begin(), normalizedMap.end(), '\\', '/');
		if (normalizedMap.empty())
		{
			return 0;
		}
		filesystem::path texPath(normalizedMap);
		if (texPath.is_relative())
		{
			texPath = selectedMtlPath.parent_path() / texPath;
		}
		else if (!filesystem::exists(texPath))
		{
			texPath = selectedMtlPath.parent_path() / texPath.filename();
		}
		if (!filesystem::exists(texPath))
		{
			return 0;
		}
		const string canonicalPath = texPath.lexically_normal().string();
		const auto cacheIt = textureCache.find(canonicalPath);
		if (cacheIt != textureCache.end())
		{
			return cacheIt->second;
		}
		int texWidth = 0;
		int texHeight = 0;
		const GLuint textureID = loadTexture(canonicalPath, texWidth, texHeight);
		textureCache[canonicalPath] = textureID;
		if (textureID != 0)
		{
			gOwnedTextureIDs.push_back(textureID);
		}
		return textureID;
	};

	auto appendVertex = [&](const OBJIndex& idx, const glm::vec3& fallbackNormal, const glm::vec3& vertexColor) -> bool
	{
		if (idx.vertex < 0 || idx.vertex >= static_cast<int>(vertices.size()))
		{
			return false;
		}

		glm::vec2 uv(0.0f, 0.0f);
		if (idx.uv >= 0 && idx.uv < static_cast<int>(texCoords.size()))
		{
			uv = texCoords[static_cast<size_t>(idx.uv)];
			outHasTexCoords = true;
		}

		glm::vec3 normal = fallbackNormal;
		if (idx.normal >= 0 && idx.normal < static_cast<int>(normals.size()))
		{
			normal = normals[static_cast<size_t>(idx.normal)];
		}
		const float normalLength = glm::length(normal);
		if (normalLength > 1e-6f)
		{
			normal /= normalLength;
		}
		else
		{
			normal = glm::vec3(0.0f, 0.0f, 1.0f);
		}

		const glm::vec3 rawPos = vertices[static_cast<size_t>(idx.vertex)];
		const glm::vec3 pos = (rawPos - modelCenter) * modelScale;
		vBuffer.push_back(pos.x);
		vBuffer.push_back(pos.y);
		vBuffer.push_back(pos.z);
		vBuffer.push_back(uv.x);
		vBuffer.push_back(uv.y);
		vBuffer.push_back(normal.x);
		vBuffer.push_back(normal.y);
		vBuffer.push_back(normal.z);
		vBuffer.push_back(vertexColor.r);
		vBuffer.push_back(vertexColor.g);
		vBuffer.push_back(vertexColor.b);
		return true;
	};

	string activeBatchKey;
	for (const TriRecord& triRecord : triangles)
	{
		const array<OBJIndex, 3>& tri = triRecord.indices;
		const string batchKey = triRecord.materialName.empty() ? "__default__" : triRecord.materialName;
		const PhongMaterial material = resolveMaterial(triRecord.materialName);
		if (gMeshBatches.empty() || batchKey != activeBatchKey)
		{
			MeshMaterialBatch batch;
			batch.first = static_cast<GLint>(vBuffer.size() / 11);
			batch.count = 0;
			batch.material = material;
			batch.textureID = resolveTextureID(material);
			batch.useTexture = (batch.textureID != 0);
			gMeshBatches.push_back(batch);
			activeBatchKey = batchKey;
		}

		glm::vec3 fallbackNormal(0.0f, 0.0f, 1.0f);
		if (tri[0].vertex >= 0 && tri[1].vertex >= 0 && tri[2].vertex >= 0
			&& tri[0].vertex < static_cast<int>(vertices.size())
			&& tri[1].vertex < static_cast<int>(vertices.size())
			&& tri[2].vertex < static_cast<int>(vertices.size()))
		{
			const glm::vec3& p0 = vertices[static_cast<size_t>(tri[0].vertex)];
			const glm::vec3& p1 = vertices[static_cast<size_t>(tri[1].vertex)];
			const glm::vec3& p2 = vertices[static_cast<size_t>(tri[2].vertex)];
			const glm::vec3 computedNormal = glm::cross(p1 - p0, p2 - p0);
			if (glm::length(computedNormal) > 1e-6f)
			{
				fallbackNormal = glm::normalize(computedNormal);
			}
		}

		const glm::vec3 vertexColor = material.kd;
		if (!appendVertex(tri[0], fallbackNormal, vertexColor) || !appendVertex(tri[1], fallbackNormal, vertexColor) || !appendVertex(tri[2], fallbackNormal, vertexColor))
		{
			cerr << "Falha ao processar face do OBJ: " << filePath << endl;
			return -1;
		}
		gMeshBatches.back().count += 3;
	}

	if (gMeshBatches.empty())
	{
		cerr << "OBJ sem lotes de material renderizaveis: " << filePath << endl;
		return -1;
	}
	outMaterial = gMeshBatches.front().material;
	outHasTexCoords = outHasTexCoords && !texCoords.empty();
	if (gMeshBatches.front().useTexture)
	{
		outTexturePath = gMeshBatches.front().material.mapKd;
	}

	GLuint VBO = 0;
	GLuint VAO = 0;
	glGenBuffers(1, &VBO);
	glBindBuffer(GL_ARRAY_BUFFER, VBO);
	glBufferData(GL_ARRAY_BUFFER, vBuffer.size() * sizeof(GLfloat), vBuffer.data(), GL_STATIC_DRAW);

	glGenVertexArrays(1, &VAO);
	glBindVertexArray(VAO);

	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 11 * sizeof(GLfloat), (GLvoid*)0);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 11 * sizeof(GLfloat), (GLvoid*)(3 * sizeof(GLfloat)));
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 11 * sizeof(GLfloat), (GLvoid*)(5 * sizeof(GLfloat)));
	glEnableVertexAttribArray(2);
	glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, 11 * sizeof(GLfloat), (GLvoid*)(8 * sizeof(GLfloat)));
	glEnableVertexAttribArray(3);

	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(0);

	nVertices = static_cast<int>(vBuffer.size() / 11);
	return static_cast<int>(VAO);
}

bool loadMeshResource(const string& name, const string& filePath, size_t& outIndex)
{
	// Carrega o OBJ e copia seus lotes de material para uma malha reutilizavel.
	int nVertices = 0;
	string texturePath;
	bool hasTexCoords = false;
	PhongMaterial firstMaterial;
	const int objVAO = loadSimpleOBJ(filePath, nVertices, texturePath, hasTexCoords, firstMaterial);
	if (objVAO == -1 || nVertices <= 0)
	{
		cerr << "Falha ao carregar malha: " << filePath << endl;
		return false;
	}

	MeshResource mesh;
	mesh.name = name;
	mesh.vao = static_cast<GLuint>(objVAO);
	mesh.vertexCount = static_cast<GLsizei>(nVertices);
	mesh.batches = gMeshBatches;
	gMeshes.push_back(mesh);
	outIndex = gMeshes.size() - 1;
	return true;
}

bool loadSceneConfig(const string& filePath)
{
	// Arquivo texto da cena: camera, luzes, malhas e instancias dos objetos.
	ifstream sceneFile(filePath.c_str());
	if (!sceneFile.is_open())
	{
		return false;
	}

	unordered_map<string, size_t> meshByName;
	string line;
	while (getline(sceneFile, line))
	{
		line = trim(line);
		if (line.empty() || line[0] == '#')
		{
			continue;
		}

		istringstream ss(line);
		string kind;
		ss >> kind;

		if (kind == "camera")
		{
			// camera x y z
			glm::vec3 position;
			if (ss >> position.x >> position.y >> position.z)
			{
				gCamera.setPosition(position);
			}
		}
		else if (kind == "light" && gLights.size() < 3)
		{
			// light x y z r g b intensidade ligado
			SceneLight light;
			int enabled = 1;
			if (ss >> light.position.x >> light.position.y >> light.position.z
				>> light.color.r >> light.color.g >> light.color.b
				>> light.intensity >> enabled)
			{
				light.enabled = (enabled != 0);
				gLights.push_back(light);
			}
		}
		else if (kind == "mesh")
		{
			// mesh nome caminho_do_obj
			string name;
			string path;
			if (ss >> name >> path)
			{
				size_t meshIndex = 0;
				if (loadMeshResource(name, path, meshIndex))
				{
					meshByName[name] = meshIndex;
				}
			}
		}
		else if (kind == "object")
		{
			// object nome mesh x y z rotX rotY rotZ escala [bezier ...]
			string name;
			string meshName;
			glm::vec3 position;
			glm::vec3 rotation;
			float scale = 1.0f;
			if (!(ss >> name >> meshName >> position.x >> position.y >> position.z >> rotation.x >> rotation.y >> rotation.z >> scale))
			{
				continue;
			}

			vector<glm::vec3> trajectory;
			string token;
			if (ss >> token && token == "bezier")
			{
				// Depois da palavra "bezier", cada trio x y z e um ponto de controle.
				glm::vec3 point;
				while (ss >> point.x >> point.y >> point.z)
				{
					trajectory.push_back(point);
				}
			}

			auto meshIt = meshByName.find(meshName);
			if (meshIt != meshByName.end())
			{
				addSceneObject(meshIt->second, name, position, rotation, scale, trajectory);
			}
		}
	}

	return !gMeshes.empty() && !gObjects.empty();
}

bool setupScene()
{
	gOwnedTextureIDs.clear();
	gMeshBatches.clear();
	gMeshes.clear();
	gObjects.clear();
	gLights.clear();

	if (loadSceneConfig("../assets/cena.txt"))
	{
		cout << "Cena carregada de ../assets/cena.txt" << endl;
		activeObjectIndex = 0;
		return true;
	}

	cerr << "Usando cena final padrao embutida." << endl;
	gOwnedTextureIDs.clear();
	gMeshBatches.clear();
	gMeshes.clear();
	gObjects.clear();
	gLights.clear();

	size_t jeepMesh = 0;
	size_t suzanneMesh = 0;
	size_t suzanneSubdivMesh = 0;
	bool ok = true;
	ok = loadMeshResource("jeep", "../../assets/Modelos3D/Jeep_Renegade_2016.obj", jeepMesh) && ok;
	ok = loadMeshResource("suzanne", "../../assets/Modelos3D/Suzanne.obj", suzanneMesh) && ok;
	ok = loadMeshResource("suzanne_subdiv", "../../assets/Modelos3D/SuzanneSubdiv1.obj", suzanneSubdivMesh) && ok;
	if (!ok)
	{
		return false;
	}

	gCamera.setPosition(glm::vec3(0.0f, 2.4f, 8.5f));
	gLights.push_back({ glm::vec3(3.8f, 4.8f, 3.2f), glm::vec3(1.0f, 0.88f, 0.70f), 1.35f, true });
	gLights.push_back({ glm::vec3(-3.4f, 2.8f, 2.4f), glm::vec3(0.45f, 0.65f, 1.0f), 0.55f, true });
	gLights.push_back({ glm::vec3(0.0f, 3.8f, -4.0f), glm::vec3(1.0f, 1.0f, 1.0f), 0.32f, true });

	addSceneObject(jeepMesh, "jeep_animado", glm::vec3(0.0f, -0.25f, 0.0f), glm::vec3(0.0f, 18.0f, 0.0f), 0.72f, {
		glm::vec3(0.0f, -0.25f, 0.0f),
		glm::vec3(-0.55f, -0.25f, -0.55f),
		glm::vec3(0.55f, -0.25f, -0.75f),
		glm::vec3(0.95f, -0.25f, 0.0f),
		glm::vec3(0.55f, -0.25f, 0.75f),
		glm::vec3(-0.55f, -0.25f, 0.55f)
	});
	addSceneObject(suzanneMesh, "suzanne", glm::vec3(1.55f, -0.18f, -0.75f), glm::vec3(0.0f, -38.0f, 0.0f), 0.36f, {});
	addSceneObject(suzanneSubdivMesh, "suzanne_subdiv", glm::vec3(-1.55f, -0.18f, -0.75f), glm::vec3(0.0f, 38.0f, 0.0f), 0.34f, {});
	activeObjectIndex = 0;
	return true;
}
