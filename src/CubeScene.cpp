/* Hello Triangle - código adaptado de https://learnopengl.com/#!Getting-started/Hello-Triangle
 *
 * Adaptado por Rossana Baptista Queiroz
 * para as disciplinas de Processamento Gráfico/Computação Gráfica - Unisinos
 * Versão inicial: 7/4/2017
 * Última atualização em 07/03/2025
 */

#include <assert.h>
#include <array>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
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
int loadSimpleOBJ(const string& filePath, int& nVertices, string& outTexturePath, bool& outHasTexCoords);
string trim(const string& value);
string loadMapKdFromMTL(const string& mtlFilePath, const string& materialName);
GLuint loadTexture(const string& filePath, int& width, int& height);

const GLuint WIDTH = 1000, HEIGHT = 1000;
const GLsizei DEFAULT_CUBE_VERTEX_COUNT = 36;
const float MOVE_STEP = 0.1f;
const float SCALE_STEP = 0.1f;
const float MIN_SCALE = 0.2f;
GLsizei gMeshVertexCount = DEFAULT_CUBE_VERTEX_COUNT;
GLuint gMeshTextureID = 0;
bool gMeshUseTexture = false;

const GLchar* vertexShaderSource = "#version 410\n"
"layout (location = 0) in vec3 position;\n"
"layout (location = 1) in vec2 texCoord;\n"
"layout (location = 2) in vec3 color;\n"
"uniform mat4 model;\n"
"uniform mat4 view;\n"
"uniform mat4 projection;\n"
"out vec2 finalTexCoord;\n"
"out vec4 finalColor;\n"
"void main()\n"
"{\n"
"gl_Position = projection * view * model * vec4(position, 1.0);\n"
"finalTexCoord = texCoord;\n"
"finalColor = vec4(color, 1.0);\n"
"}\0";

const GLchar* fragmentShaderSource = "#version 410\n"
"in vec2 finalTexCoord;\n"
"in vec4 finalColor;\n"
"uniform sampler2D texBuff;\n"
"uniform bool useTexture;\n"
"out vec4 color;\n"
"void main()\n"
"{\n"
"if (useTexture)\n"
"{\n"
"color = texture(texBuff, finalTexCoord);\n"
"}\n"
"else\n"
"{\n"
"color = finalColor;\n"
"}\n"
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

	GLuint shaderID = setupShader();
	GLuint VAO = setupGeometry();

	glUseProgram(shaderID);

	GLint modelLoc = glGetUniformLocation(shaderID, "model");
	GLint viewLoc = glGetUniformLocation(shaderID, "view");
	GLint projectionLoc = glGetUniformLocation(shaderID, "projection");
	GLint texBuffLoc = glGetUniformLocation(shaderID, "texBuff");
	GLint useTextureLoc = glGetUniformLocation(shaderID, "useTexture");

	glm::mat4 view = glm::lookAt(
		glm::vec3(0.0f, 2.0f, 7.0f),
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
	glUniform1i(useTextureLoc, gMeshUseTexture ? 1 : 0);

	glEnable(GL_DEPTH_TEST);

	addCube();

	while (!glfwWindowShouldClose(window))
	{
		glfwPollEvents();

		glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		glLineWidth(10);
		glPointSize(8);

		const float angle = static_cast<GLfloat>(glfwGetTime());

		glBindVertexArray(VAO);
		if (gMeshUseTexture)
		{
			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, gMeshTextureID);
		}
		else
		{
			glBindTexture(GL_TEXTURE_2D, 0);
		}
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
			glDrawArrays(GL_TRIANGLES, 0, gMeshVertexCount);
			glDrawArrays(GL_POINTS, 0, gMeshVertexCount);
		}
		glBindVertexArray(0);

		glfwSwapBuffers(window);
	}

	if (gMeshTextureID != 0)
	{
		glDeleteTextures(1, &gMeshTextureID);
	}
	glDeleteVertexArrays(1, &VAO);
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

string loadMapKdFromMTL(const string& mtlFilePath, const string& materialName)
{
	ifstream mtlFile(mtlFilePath.c_str());
	if (!mtlFile.is_open())
	{
		cerr << "Falha ao abrir MTL: " << mtlFilePath << endl;
		return "";
	}

	string line;
	string currentMaterial;
	string firstMapKd;
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
			string rest;
			getline(ss, rest);
			currentMaterial = trim(rest);
		}
		else if (word == "map_Kd")
		{
			string rest;
			getline(ss, rest);
			string mapKd = trim(rest);
			if (mapKd.empty())
			{
				continue;
			}
			if (!materialName.empty() && currentMaterial == materialName)
			{
				return mapKd;
			}
			if (firstMapKd.empty())
			{
				firstMapKd = mapKd;
			}
		}
	}

	return firstMapKd;
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

int loadSimpleOBJ(const string& filePath, int& nVertices, string& outTexturePath, bool& outHasTexCoords)
{
	struct OBJIndex
	{
		int vertex = -1;
		int uv = -1;
	};

	vector<glm::vec3> vertices;
	vector<glm::vec2> texCoords;
	vector<GLfloat> vBuffer;
	const glm::vec3 fallbackColor(1.0f, 1.0f, 1.0f);

	string mtlFileName;
	string currentMaterial;
	string firstMaterialUsed;
	outTexturePath.clear();
	outHasTexCoords = false;

	ifstream arqEntrada(filePath.c_str());
	if (!arqEntrada.is_open())
	{
		cerr << "Erro ao tentar ler o arquivo " << filePath << endl;
		return -1;
	}

	auto appendVertex = [&](const OBJIndex& idx) -> bool
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

		const glm::vec3& pos = vertices[static_cast<size_t>(idx.vertex)];
		vBuffer.push_back(pos.x);
		vBuffer.push_back(pos.y);
		vBuffer.push_back(pos.z);
		vBuffer.push_back(uv.x);
		vBuffer.push_back(uv.y);
		vBuffer.push_back(fallbackColor.r);
		vBuffer.push_back(fallbackColor.g);
		vBuffer.push_back(fallbackColor.b);
		return true;
	};

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
		}
		else if (word == "vt")
		{
			glm::vec2 vt;
			ssline >> vt.x >> vt.y;
			texCoords.push_back(vt);
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
				face.push_back(idx);
			}

			if (face.size() < 3)
			{
				continue;
			}

			for (size_t i = 1; i + 1 < face.size(); ++i)
			{
				if (!appendVertex(face[0]) || !appendVertex(face[i]) || !appendVertex(face[i + 1]))
				{
					cerr << "Falha ao processar face do OBJ: " << filePath << endl;
					return -1;
				}
			}
		}
	}

	if (!mtlFileName.empty())
	{
		filesystem::path objPath(filePath);
		filesystem::path mtlPath = objPath.parent_path() / mtlFileName;
		string mapKd = loadMapKdFromMTL(mtlPath.string(), firstMaterialUsed);
		if (!mapKd.empty())
		{
			filesystem::path texPath(mapKd);
			if (texPath.is_relative())
			{
				texPath = mtlPath.parent_path() / texPath;
			}
			outTexturePath = texPath.lexically_normal().string();
		}
	}

	if (vBuffer.empty())
	{
		cerr << "OBJ sem vertices renderizaveis: " << filePath << endl;
		return -1;
	}

	GLuint VBO = 0;
	GLuint VAO = 0;
	glGenBuffers(1, &VBO);
	glBindBuffer(GL_ARRAY_BUFFER, VBO);
	glBufferData(GL_ARRAY_BUFFER, vBuffer.size() * sizeof(GLfloat), vBuffer.data(), GL_STATIC_DRAW);

	glGenVertexArrays(1, &VAO);
	glBindVertexArray(VAO);

	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(GLfloat), (GLvoid*)0);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(GLfloat), (GLvoid*)(3 * sizeof(GLfloat)));
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(GLfloat), (GLvoid*)(5 * sizeof(GLfloat)));
	glEnableVertexAttribArray(2);

	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(0);

	nVertices = static_cast<int>(vBuffer.size() / 8);
	return static_cast<int>(VAO);
}

int setupGeometry()
{
	int nVertices = 0;
	string texturePath;
	bool hasTexCoords = false;
	const int objVAO = loadSimpleOBJ("../assets/Modelos3D/Suzanne.obj", nVertices, texturePath, hasTexCoords);
	if (objVAO != -1 && nVertices > 0)
	{
		gMeshVertexCount = static_cast<GLsizei>(nVertices);
		if (hasTexCoords && !texturePath.empty())
		{
			int texWidth = 0;
			int texHeight = 0;
			gMeshTextureID = loadTexture(texturePath, texWidth, texHeight);
			gMeshUseTexture = (gMeshTextureID != 0);
		}
		else
		{
			gMeshUseTexture = false;
		}

		if (!gMeshUseTexture)
		{
			cerr << "OBJ carregado sem textura valida. Mantendo renderizacao por cor." << endl;
		}
		return objVAO;
	}

	cerr << "Falha no OBJ. Usando cubo hardcoded de fallback." << endl;
	gMeshVertexCount = DEFAULT_CUBE_VERTEX_COUNT;
	gMeshUseTexture = false;
	gMeshTextureID = 0;

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

	GLuint VBO, VAO;

	glGenBuffers(1, &VBO);
	glBindBuffer(GL_ARRAY_BUFFER, VBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

	glGenVertexArrays(1, &VAO);
	glBindVertexArray(VAO);

	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(GLfloat), (GLvoid*)0);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(GLfloat), (GLvoid*)(3 * sizeof(GLfloat)));
	glEnableVertexAttribArray(2);

	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(0);

	return VAO;
}
