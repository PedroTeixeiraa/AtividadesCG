cmake_minimum_required(VERSION 3.10)
project(GrauBJeepRenegade)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

include(FetchContent)

list(APPEND CMAKE_PREFIX_PATH /opt/homebrew)

FetchContent_Declare(
  glm
  GIT_REPOSITORY https://github.com/g-truc/glm.git
  GIT_TAG master
)

FetchContent_Declare(
  stb_image
  GIT_REPOSITORY https://github.com/nothings/stb.git
  GIT_TAG master
)

FetchContent_MakeAvailable(glm stb_image)

find_package(glfw3 REQUIRED)

if(WIN32)
    set(OPENGL_LIBS opengl32)
elseif(APPLE)
    find_library(OpenGL_LIBRARY OpenGL)
    set(OPENGL_LIBS ${OpenGL_LIBRARY})
else()
    find_package(OpenGL REQUIRED)
    set(OPENGL_LIBS ${OPENGL_gl_LIBRARY})
endif()

set(ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/..")
set(GLAD_C_FILE "${ROOT_DIR}/Common/glad.c")

if (NOT EXISTS ${GLAD_C_FILE})
    message(FATAL_ERROR "Arquivo Common/glad.c nao encontrado na raiz do projeto.")
endif()

add_executable(JeepRenegade src/JeepRenegade.cpp ${GLAD_C_FILE})
target_include_directories(JeepRenegade PRIVATE
    ${ROOT_DIR}/include
    ${ROOT_DIR}/include/glad
    ${glm_SOURCE_DIR}
    ${stb_image_SOURCE_DIR}
)
target_link_libraries(JeepRenegade glfw ${OPENGL_LIBS})

