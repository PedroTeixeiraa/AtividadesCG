/* Hello Triangle - código adaptado de https://learnopengl.com/#!Getting-started/Hello-Triangle
 *
 * Adaptado por Rossana Baptista Queiroz
 * para as disciplinas de Processamento Gráfico/Computação Gráfica - Unisinos
 * Versão inicial: 7/4/2017
 * Última atualização em 07/03/2025
 */

#include <assert.h>
#include <algorithm>
#include <array>
#include <cstdlib>
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
int setupShader();
int setupGeometry();

struct PhongMaterial
{
	glm::vec3 ka = glm::vec3(0.1f, 0.1f, 0.1f);
	glm::vec3 kd = glm::vec3(1.0f, 1.0f, 1.0f);
	glm::vec3 ks = glm::vec3(0.5f, 0.5f, 0.5f);
	float ns = 32.0f;
	string mapKd;
};

struct MeshMaterialBatch
{
	GLint first = 0;
	GLsizei count = 0;
	PhongMaterial material;
	GLuint textureID = 0;
	bool useTexture = false;
};

bool loadPhongMaterialsFromMTL(const string& mtlFilePath, unordered_map<string, PhongMaterial>& outMaterials, vector<string>& outOrder);
int loadSimpleOBJ(const string& filePath, int& nVertices, string& outTexturePath, bool& outHasTexCoords, PhongMaterial& outMaterial);
string trim(const string& value);
GLuint loadTexture(const string& filePath, int& width, int& height);

const GLuint WIDTH = 1000, HEIGHT = 1000;
const GLsizei DEFAULT_CUBE_VERTEX_COUNT = 36;
const float MOVE_STEP = 0.1f;
const float SCALE_STEP = 0.1f;
const float MIN_SCALE = 0.2f;
GLsizei gMeshVertexCount = DEFAULT_CUBE_VERTEX_COUNT;
vector<MeshMaterialBatch> gMeshBatches;
vector<GLuint> gOwnedTextureIDs;
GLuint gFallbackTextureID = 0;

const glm::vec3 LIGHT_POSITION(6.0f, 2.5f, 1.5f);
const glm::vec3 LIGHT_COLOR(1.0f, 1.0f, 1.0f);
const glm::vec3 CAMERA_POSITION(0.0f, 2.0f, 7.0f);
const float PHONG_AMBIENT_GAIN = 0.45f;
const float PHONG_DIFFUSE_GAIN = 1.25f;
const float PHONG_SPECULAR_GAIN = 2.8f;

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

const GLchar* fragmentShaderSource = "#version 410\n"
"in vec2 finalTexCoord;\n"
"in vec3 finalNormal;\n"
"in vec3 fragPos;\n"
"in vec3 finalColor;\n"
"uniform sampler2D texBuff;\n"
"uniform bool useTexture;\n"
"uniform vec3 lightPos;\n"
"uniform vec3 lightColor;\n"
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
"vec3 baseColor = useTexture ? texture(texBuff, finalTexCoord).rgb : finalColor;\n"
"vec3 N = normalize(finalNormal);\n"
"vec3 L = normalize(lightPos - fragPos);\n"
"vec3 V = normalize(camPos - fragPos);\n"
"vec3 ambient = ambientGain * matKa * lightColor;\n"
"float diff = max(dot(N, L), 0.0);\n"
"vec3 diffuse = diffuseGain * matKd * diff * lightColor;\n"
"vec3 specular = vec3(0.0);\n"
"if (diff > 0.0)\n"
"{\n"
"vec3 R = reflect(-L, N);\n"
"float spec = pow(max(dot(V, R), 0.0), max(matNs * 1.5, 1.0));\n"
"specular = specularGain * matKs * spec * lightColor;\n"
"}\n"
"vec3 result = (ambient + diffuse) * baseColor + specular;\n"
"color = vec4(result, 1.0);\n"
"}\n\0";

enum RotationAxis
{
	AXIS_NONE = 0,
	AXIS_X = 1,
	AXIS_Y = 2,
	AXIS_Z = 3
};

struct CubeInstance
{
	glm::vec3 position;
	float scale;
	RotationAxis rotationAxis;
};

vector<CubeInstance> cubes;
size_t activeCubeIndex = 0;

CubeInstance& activeCube()
{
	return cubes[activeCubeIndex];
}

void addCube()
{
	const int columns = 3;
	const float spacing = 1.8f;
	const size_t index = cubes.size();
	const int col = static_cast<int>(index % columns);
	const int row = static_cast<int>(index / columns);

	CubeInstance instance;
	instance.position = glm::vec3((static_cast<float>(col) - 1.0f) * spacing, 0.0f, -static_cast<float>(row) * spacing);
	instance.scale = 1.0f;
	instance.rotationAxis = AXIS_NONE;
	cubes.push_back(instance);
	activeCubeIndex = cubes.size() - 1;
}

void cycleActiveCube()
{
	if (cubes.empty())
	{
		return;
	}
	activeCubeIndex = (activeCubeIndex + 1) % cubes.size();
}

void moveActiveCube(const glm::vec3& delta)
{
	if (cubes.empty())
	{
		return;
	}
	activeCube().position += delta;
}

void scaleActiveCube(float delta)
{
	if (cubes.empty())
	{
		return;
	}
	activeCube().scale += delta;
	if (activeCube().scale < MIN_SCALE)
	{
		activeCube().scale = MIN_SCALE;
	}
}

void setActiveRotationAxis(RotationAxis axis)
{
	if (cubes.empty())
	{
		return;
	}
	activeCube().rotationAxis = axis;
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

	GLFWwindow* window = glfwCreateWindow(WIDTH, HEIGHT, "Cubo 3D -- Pedro Teixeira Alves!", nullptr, nullptr);
	glfwMakeContextCurrent(window);
	glfwSetKeyCallback(window, key_callback);

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
	glGenTextures(1, &gFallbackTextureID);
	glBindTexture(GL_TEXTURE_2D, gFallbackTextureID);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, whitePixel);
	glBindTexture(GL_TEXTURE_2D, 0);

	GLuint shaderID = setupShader();
	GLuint VAO = setupGeometry();

	glUseProgram(shaderID);

	GLint modelLoc = glGetUniformLocation(shaderID, "model");
	GLint viewLoc = glGetUniformLocation(shaderID, "view");
	GLint projectionLoc = glGetUniformLocation(shaderID, "projection");
	GLint texBuffLoc = glGetUniformLocation(shaderID, "texBuff");
	GLint useTextureLoc = glGetUniformLocation(shaderID, "useTexture");
	GLint lightPosLoc = glGetUniformLocation(shaderID, "lightPos");
	GLint lightColorLoc = glGetUniformLocation(shaderID, "lightColor");
	GLint camPosLoc = glGetUniformLocation(shaderID, "camPos");
	GLint matKaLoc = glGetUniformLocation(shaderID, "matKa");
	GLint matKdLoc = glGetUniformLocation(shaderID, "matKd");
	GLint matKsLoc = glGetUniformLocation(shaderID, "matKs");
	GLint matNsLoc = glGetUniformLocation(shaderID, "matNs");
	GLint ambientGainLoc = glGetUniformLocation(shaderID, "ambientGain");
	GLint diffuseGainLoc = glGetUniformLocation(shaderID, "diffuseGain");
	GLint specularGainLoc = glGetUniformLocation(shaderID, "specularGain");

	glm::mat4 view = glm::lookAt(
		CAMERA_POSITION,
		glm::vec3(0.0f, 0.0f, 0.0f),
		glm::vec3(0.0f, 1.0f, 0.0f)
	);
	glm::mat4 projection = glm::perspective(
		glm::radians(45.0f),
		static_cast<float>(width) / static_cast<float>(height),
		0.1f,
		100.0f
	);

	glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(view));
	glUniformMatrix4fv(projectionLoc, 1, GL_FALSE, glm::value_ptr(projection));
	glUniform1i(texBuffLoc, 0);
	glUniform1i(useTextureLoc, 0);
	glUniform3fv(lightPosLoc, 1, glm::value_ptr(LIGHT_POSITION));
	glUniform3fv(lightColorLoc, 1, glm::value_ptr(LIGHT_COLOR));
	glUniform3fv(camPosLoc, 1, glm::value_ptr(CAMERA_POSITION));
	glUniform1f(ambientGainLoc, PHONG_AMBIENT_GAIN);
	glUniform1f(diffuseGainLoc, PHONG_DIFFUSE_GAIN);
	glUniform1f(specularGainLoc, PHONG_SPECULAR_GAIN);

	glEnable(GL_DEPTH_TEST);

	addCube();

	while (!glfwWindowShouldClose(window))
	{
		glfwPollEvents();

		glClearColor(0.10f, 0.10f, 0.12f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		glLineWidth(10);
		glPointSize(8);

		const float angle = static_cast<GLfloat>(glfwGetTime());

		glBindVertexArray(VAO);
		glActiveTexture(GL_TEXTURE0);
		for (const CubeInstance& cube : cubes)
		{
			glm::mat4 model = glm::mat4(1.0f);
			model = glm::translate(model, cube.position);

			if (cube.rotationAxis == AXIS_X)
			{
				model = glm::rotate(model, angle, glm::vec3(1.0f, 0.0f, 0.0f));
			}
			else if (cube.rotationAxis == AXIS_Y)
			{
				model = glm::rotate(model, angle, glm::vec3(0.0f, 1.0f, 0.0f));
			}
			else if (cube.rotationAxis == AXIS_Z)
			{
				model = glm::rotate(model, angle, glm::vec3(0.0f, 0.0f, 1.0f));
			}

			model = glm::scale(model, glm::vec3(cube.scale));

			glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));
			for (const MeshMaterialBatch& batch : gMeshBatches)
			{
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
	glDeleteVertexArrays(1, &VAO);
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
	else if (key == GLFW_KEY_A)
	{
		moveActiveCube(glm::vec3(-MOVE_STEP, 0.0f, 0.0f));
	}
	else if (key == GLFW_KEY_D)
	{
		moveActiveCube(glm::vec3(MOVE_STEP, 0.0f, 0.0f));
	}
	else if (key == GLFW_KEY_W)
	{
		moveActiveCube(glm::vec3(0.0f, 0.0f, -MOVE_STEP));
	}
	else if (key == GLFW_KEY_S)
	{
		moveActiveCube(glm::vec3(0.0f, 0.0f, MOVE_STEP));
	}
	else if (key == GLFW_KEY_I)
	{
		moveActiveCube(glm::vec3(0.0f, MOVE_STEP, 0.0f));
	}
	else if (key == GLFW_KEY_J)
	{
		moveActiveCube(glm::vec3(0.0f, -MOVE_STEP, 0.0f));
	}
	else if (key == GLFW_KEY_LEFT_BRACKET)
	{
		scaleActiveCube(-SCALE_STEP);
	}
	else if (key == GLFW_KEY_RIGHT_BRACKET)
	{
		scaleActiveCube(SCALE_STEP);
	}
	else if (key == GLFW_KEY_N && action == GLFW_PRESS)
	{
		addCube();
	}
	else if (key == GLFW_KEY_TAB && action == GLFW_PRESS)
	{
		cycleActiveCube();
	}
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

int setupGeometry()
{
	gOwnedTextureIDs.clear();
	gMeshBatches.clear();

	int nVertices = 0;
	string texturePath;
	bool hasTexCoords = false;
	PhongMaterial firstMaterial;
	const int objVAO = loadSimpleOBJ("../assets/Modelos3D/Car.obj", nVertices, texturePath, hasTexCoords, firstMaterial);
	if (objVAO != -1 && nVertices > 0)
	{
		gMeshVertexCount = static_cast<GLsizei>(nVertices);
		bool hasAnyTexture = false;
		for (const MeshMaterialBatch& batch : gMeshBatches)
		{
			if (batch.useTexture)
			{
				hasAnyTexture = true;
				break;
			}
		}
		if (!hasAnyTexture)
		{
			cerr << "OBJ carregado sem textura valida. Mantendo renderizacao por cor." << endl;
		}
		return objVAO;
	}

	cerr << "Falha no OBJ. Usando cubo hardcoded de fallback." << endl;
	gMeshVertexCount = DEFAULT_CUBE_VERTEX_COUNT;

	GLfloat vertices[] = {
		// Frente (vermelho)
		-0.5f, -0.5f,  0.5f, 1.0f, 0.0f, 0.0f,
		 0.5f, -0.5f,  0.5f, 1.0f, 0.0f, 0.0f,
		 0.5f,  0.5f,  0.5f, 1.0f, 0.0f, 0.0f,
		 0.5f,  0.5f,  0.5f, 1.0f, 0.0f, 0.0f,
		-0.5f,  0.5f,  0.5f, 1.0f, 0.0f, 0.0f,
		-0.5f, -0.5f,  0.5f, 1.0f, 0.0f, 0.0f,

		// Traseira (verde)
		 0.5f, -0.5f, -0.5f, 0.0f, 1.0f, 0.0f,
		-0.5f, -0.5f, -0.5f, 0.0f, 1.0f, 0.0f,
		-0.5f,  0.5f, -0.5f, 0.0f, 1.0f, 0.0f,
		-0.5f,  0.5f, -0.5f, 0.0f, 1.0f, 0.0f,
		 0.5f,  0.5f, -0.5f, 0.0f, 1.0f, 0.0f,
		 0.5f, -0.5f, -0.5f, 0.0f, 1.0f, 0.0f,

		// Esquerda (azul)
		-0.5f, -0.5f, -0.5f, 0.0f, 0.0f, 1.0f,
		-0.5f, -0.5f,  0.5f, 0.0f, 0.0f, 1.0f,
		-0.5f,  0.5f,  0.5f, 0.0f, 0.0f, 1.0f,
		-0.5f,  0.5f,  0.5f, 0.0f, 0.0f, 1.0f,
		-0.5f,  0.5f, -0.5f, 0.0f, 0.0f, 1.0f,
		-0.5f, -0.5f, -0.5f, 0.0f, 0.0f, 1.0f,

		// Direita (amarelo)
		 0.5f, -0.5f,  0.5f, 1.0f, 1.0f, 0.0f,
		 0.5f, -0.5f, -0.5f, 1.0f, 1.0f, 0.0f,
		 0.5f,  0.5f, -0.5f, 1.0f, 1.0f, 0.0f,
		 0.5f,  0.5f, -0.5f, 1.0f, 1.0f, 0.0f,
		 0.5f,  0.5f,  0.5f, 1.0f, 1.0f, 0.0f,
		 0.5f, -0.5f,  0.5f, 1.0f, 1.0f, 0.0f,

		// Topo (magenta)
		-0.5f,  0.5f,  0.5f, 1.0f, 0.0f, 1.0f,
		 0.5f,  0.5f,  0.5f, 1.0f, 0.0f, 1.0f,
		 0.5f,  0.5f, -0.5f, 1.0f, 0.0f, 1.0f,
		 0.5f,  0.5f, -0.5f, 1.0f, 0.0f, 1.0f,
		-0.5f,  0.5f, -0.5f, 1.0f, 0.0f, 1.0f,
		-0.5f,  0.5f,  0.5f, 1.0f, 0.0f, 1.0f,

		// Base (ciano)
		-0.5f, -0.5f, -0.5f, 0.0f, 1.0f, 1.0f,
		 0.5f, -0.5f, -0.5f, 0.0f, 1.0f, 1.0f,
		 0.5f, -0.5f,  0.5f, 0.0f, 1.0f, 1.0f,
		 0.5f, -0.5f,  0.5f, 0.0f, 1.0f, 1.0f,
		-0.5f, -0.5f,  0.5f, 0.0f, 1.0f, 1.0f,
		-0.5f, -0.5f, -0.5f, 0.0f, 1.0f, 1.0f
	};

	vector<GLfloat> fallbackBuffer;
	const size_t sourceStride = 6;
	const size_t sourceCount = sizeof(vertices) / sizeof(GLfloat);
	for (size_t i = 0; i + (3 * sourceStride) <= sourceCount; i += 3 * sourceStride)
	{
		const glm::vec3 p0(vertices[i], vertices[i + 1], vertices[i + 2]);
		const glm::vec3 p1(vertices[i + sourceStride], vertices[i + sourceStride + 1], vertices[i + sourceStride + 2]);
		const glm::vec3 p2(vertices[i + 2 * sourceStride], vertices[i + 2 * sourceStride + 1], vertices[i + 2 * sourceStride + 2]);
		glm::vec3 normal(0.0f, 0.0f, 1.0f);
		const glm::vec3 computedNormal = glm::cross(p1 - p0, p2 - p0);
		if (glm::length(computedNormal) > 1e-6f)
		{
			normal = glm::normalize(computedNormal);
		}

		for (size_t vertexOffset = 0; vertexOffset < 3; ++vertexOffset)
		{
			const size_t base = i + vertexOffset * sourceStride;
			fallbackBuffer.push_back(vertices[base]);
			fallbackBuffer.push_back(vertices[base + 1]);
			fallbackBuffer.push_back(vertices[base + 2]);
			fallbackBuffer.push_back(0.0f);
			fallbackBuffer.push_back(0.0f);
			fallbackBuffer.push_back(normal.x);
			fallbackBuffer.push_back(normal.y);
			fallbackBuffer.push_back(normal.z);
			fallbackBuffer.push_back(vertices[base + 3]);
			fallbackBuffer.push_back(vertices[base + 4]);
			fallbackBuffer.push_back(vertices[base + 5]);
		}
	}
	gMeshVertexCount = static_cast<GLsizei>(fallbackBuffer.size() / 11);
	gMeshBatches.push_back(MeshMaterialBatch());
	gMeshBatches.back().first = 0;
	gMeshBatches.back().count = gMeshVertexCount;
	gMeshBatches.back().material = PhongMaterial();
	gMeshBatches.back().useTexture = false;
	gMeshBatches.back().textureID = 0;

	GLuint VBO, VAO;

	glGenBuffers(1, &VBO);
	glBindBuffer(GL_ARRAY_BUFFER, VBO);
	glBufferData(GL_ARRAY_BUFFER, fallbackBuffer.size() * sizeof(GLfloat), fallbackBuffer.data(), GL_STATIC_DRAW);

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

	return VAO;
}
