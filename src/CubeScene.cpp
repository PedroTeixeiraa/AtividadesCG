/* Hello Triangle - código adaptado de https://learnopengl.com/#!Getting-started/Hello-Triangle
 *
 * Adaptado por Rossana Baptista Queiroz
 * para as disciplinas de Processamento Gráfico/Computação Gráfica - Unisinos
 * Versão inicial: 7/4/2017
 * Última atualização em 07/03/2025
 */

#include <assert.h>
#include <iostream>
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

void key_callback(GLFWwindow* window, int key, int scancode, int action, int mode);
int setupShader();
int setupGeometry();

const GLuint WIDTH = 1000, HEIGHT = 1000;
const GLsizei CUBE_VERTEX_COUNT = 36;
const float MOVE_STEP = 0.1f;
const float SCALE_STEP = 0.1f;
const float MIN_SCALE = 0.2f;

const GLchar* vertexShaderSource = "#version 410\n"
"layout (location = 0) in vec3 position;\n"
"layout (location = 1) in vec3 color;\n"
"uniform mat4 model;\n"
"uniform mat4 view;\n"
"uniform mat4 projection;\n"
"out vec4 finalColor;\n"
"void main()\n"
"{\n"
"gl_Position = projection * view * model * vec4(position, 1.0);\n"
"finalColor = vec4(color, 1.0);\n"
"}\0";

const GLchar* fragmentShaderSource = "#version 410\n"
"in vec4 finalColor;\n"
"out vec4 color;\n"
"void main()\n"
"{\n"
"color = finalColor;\n"
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
			glDrawArrays(GL_TRIANGLES, 0, CUBE_VERTEX_COUNT);
			glDrawArrays(GL_POINTS, 0, CUBE_VERTEX_COUNT);
		}
		glBindVertexArray(0);

		glfwSwapBuffers(window);
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

int setupGeometry()
{
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
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(GLfloat), (GLvoid*)(3 * sizeof(GLfloat)));
	glEnableVertexAttribArray(1);

	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(0);

	return VAO;
}
