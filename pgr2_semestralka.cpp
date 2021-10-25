#define STB_IMAGE_IMPLEMENTATION
#define _CRT_SECURE_NO_WARNINGS

#include <iostream>
#include "stb_image.h"
#include "handler.h"
#include "camera.h"
#include "shapes.h"
#include <thread> 
#include <mutex>
#define GLFW_INCLUDE_NONE
#include "glad/glad.h"
#include "GLFW/glfw3.h"
#include "glm/gtc/type_ptr.hpp"
#include <process.h>
#include <chrono>


struct Handler handler {};

namespace pgr {

    void checkGLError(const char* where, int line) {
        GLenum err = glGetError();
        if (err == GL_NONE)
            return;

        std::string errString = "<unknown>";
        switch (err) {
        case GL_INVALID_ENUM:
            errString = "GL_INVALID_ENUM";
            break;
        case GL_INVALID_VALUE:
            errString = "GL_INVALID_VALUE";
            break;
        case GL_INVALID_OPERATION:
            errString = "GL_INVALID_OPERATION";
            break;
        case GL_INVALID_FRAMEBUFFER_OPERATION:
            errString = "GL_INVALID_FRAMEBUFFER_OPERATION";
            break;
        case GL_OUT_OF_MEMORY:
            errString = "GL_OUT_OF_MEMORY";
            break;
        default:;
        }
        if (where == 0 || *where == 0)
            std::cerr << "GL error occurred: " << errString << std::endl;
        else
            std::cerr << "GL error occurred in " << where << ":" << line << ": " << errString << std::endl;
    }

#define CHECK_GL_ERROR() do { pgr::checkGLError(__FUNCTION__, __LINE__); } while(0)

    GLuint createShaderFromSource(GLenum eShaderType, const std::string& strShaderText) {
        GLuint shader = glCreateShader(eShaderType);
        const char* strFileData = strShaderText.c_str();
        glShaderSource(shader, 1, &strFileData, NULL);

        glCompileShader(shader);

        GLint status;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
        if (status == GL_FALSE) {
            GLint infoLogLength;
            glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &infoLogLength);

            GLchar* strInfoLog = new GLchar[infoLogLength + 1];
            glGetShaderInfoLog(shader, infoLogLength, NULL, strInfoLog);

            const char* strShaderType = NULL;
            switch (eShaderType) {
            case GL_VERTEX_SHADER: strShaderType = "vertex";   break;
            case GL_FRAGMENT_SHADER: strShaderType = "fragment"; break;
            case GL_GEOMETRY_SHADER: strShaderType = "geometry"; break;
            }

            std::cerr << "Compile failure in " << strShaderType << " shader:" << std::endl;
            std::cerr << strInfoLog;

            delete[] strInfoLog;
            glDeleteShader(shader);
            return 0;
        }

        return shader;
    }

    GLuint createShaderFromFile(GLenum eShaderType, const std::string& filename) {
        FILE* f = fopen(filename.c_str(), "rb");
        if (!f) {
            std::cerr << "Unable to open file " << filename << " for reading" << std::endl;
            return 0;
        }
        else
            std::cout << "loading shader: " << filename << std::endl;

        fseek(f, 0, SEEK_END);
        int length = ftell(f);
        fseek(f, 0, SEEK_SET);

        char* buffer = new char[length + 1];
        int length2 = fread(buffer, 1, length, f);
        fclose(f);
        buffer[length] = '\0';

        GLuint sh = createShaderFromSource(eShaderType, buffer);
        delete[] buffer;
        return sh;
    }

    static bool linkProgram(GLuint program) {
        glLinkProgram(program);

        GLint status = GL_FALSE;
        glGetProgramiv(program, GL_LINK_STATUS, &status);
        CHECK_GL_ERROR();

        if (status == GL_FALSE) {
            GLint infoLogLength;
            glGetProgramiv(program, GL_INFO_LOG_LENGTH, &infoLogLength);

            GLchar* strInfoLog = new GLchar[infoLogLength + 1];
            glGetProgramInfoLog(program, infoLogLength, NULL, strInfoLog);
            fprintf(stderr, "Linker failure: %s\n", strInfoLog);
            delete[] strInfoLog;
            return false;
        }

        return true;
    }

    GLuint createProgram(const GLuint* shaders) {
        GLuint program = glCreateProgram();

        while (shaders && *shaders)
            glAttachShader(program, *shaders++);
        CHECK_GL_ERROR();

        if (!linkProgram(program)) {
            return 0;
        }

        return program;
    }

}



const unsigned int maxCounter = 10;

unsigned long frameTimer;
GLuint64 timer;
GLuint textureQueries[maxCounter * handler.nTextures];
GLuint vertexQueries[maxCounter];
unsigned int thisFrameIndex = 0;

bool drawTextures = true;

bool changeMethod = false;
unsigned char newMethod = 0;

bool changeTexture = false;
bool endTextureMethod = false;

constexpr unsigned int MS_PER_FRAME = 33;

camera cam;

const unsigned int numberOfCubeSubbuffers = 3;
std::thread bufferThread;
bool bufferThreadEnd = false;

std::mutex secondMethodMutexCamera;
std::mutex secondMethodMutexData[numberOfCubeSubbuffers];
std::mutex thidMethodMutex[numberOfCubeSubbuffers];
GLsync thirdMethodSyncUploadStart[numberOfCubeSubbuffers];
GLsync thirdMethodSyncUploadEnd[numberOfCubeSubbuffers];

unsigned char bufferMethod = 0;
bool bufferMethod2Updated = false;

bool bufferMapped = false;
bool end = false;

bool useAsynchTextures = false;
bool textureLoaded[handler.nTextures];
unsigned int preloadedTextures = 2;
bool textureThreadWasStarted = false;

GLsync startUpload[handler.nTextures];
std::mutex startUploadMutex[handler.nTextures];
std::mutex endUploadMutex[handler.nTextures];
bool firstTextureLoaded = false;
GLsync endUpload[handler.nTextures];
std::thread textureThread;
bool textureThreadRunning = false;
GLuint pbo[2];
unsigned int curPBO = 0;

GLfloat* cubesMappedPointer;
const unsigned int cubeSize = sizeof(cubeVertices);
const unsigned int nCubeTriangles = cubeSize / 3 / sizeof(GLfloat);
const unsigned int nCubesRow = 100;
const unsigned int nCubesCol = 100;
const unsigned int nCubesDepth = 100;
const float diff = 3.0f;
const unsigned int cubesSize = nCubeTriangles * nCubesRow * nCubesCol * nCubesDepth;

unsigned int numberOfCubesPreComputed = 1;
GLuint* cubesIndices;

unsigned int cubeDrawingIndex = numberOfCubeSubbuffers - 1;

void fillCubeArray(GLfloat*);

void updateCommonUniforms(int i) {
    glm::mat4 projection = glm::perspectiveFov(70.0f, float(handler.windowWidth), float(handler.windowHeight), 1.0f, 200.0f);


    secondMethodMutexCamera.lock();
    glm::mat4 view = glm::lookAt(cam.getPosition(), cam.getPosition() + cam.getDirection(), cam.getUpVector());
    secondMethodMutexCamera.unlock();

    glm::mat4 modelMatrix = glm::scale(glm::mat4(1.0f), glm::vec3(5.0f)) + glm::translate(glm::mat4(1.0f), glm::vec3(0, 0, -i * 15.0f));
    glUseProgram(handler.program);
    glm::mat4 pvm = projection * view * modelMatrix;
    CHECK_GL_ERROR();
    glUniformMatrix4fv(handler.pvmMatrix, 1, GL_FALSE, glm::value_ptr(pvm));
    glUniformMatrix4fv(handler.mMatrix, 1, GL_FALSE, glm::value_ptr(modelMatrix));
    glUniformMatrix4fv(handler.vMatrix, 1, GL_FALSE, glm::value_ptr(view));
    glUniformMatrix4fv(handler.vmMatrix, 1, GL_FALSE, glm::value_ptr(view * modelMatrix));

    CHECK_GL_ERROR();
    glm::mat4 nMatrix = glm::transpose(glm::inverse(modelMatrix));
    CHECK_GL_ERROR();
    glUniformMatrix3fv(handler.nMatrix, 1, GL_FALSE, glm::value_ptr(glm::mat3(nMatrix)));
    CHECK_GL_ERROR();
}


void drawSquareAsync() {
    CHECK_GL_ERROR();
    for (int i = 0; i < handler.nTextures; i++)
    {
        // we cannot draw textures before they are uploaded
        if (!firstTextureLoaded) {
            return;
        }

        // update uniform matrices
        updateCommonUniforms(i);

        // set a uniform that tells if we use texture
        glUniform1i(handler.useEmissionTexture, 1);

        // begin timing
        glBeginQuery(GL_TIME_ELAPSED, textureQueries[thisFrameIndex++]);
        CHECK_GL_ERROR();

        // locks that prevent the other thread from changing texture that is drawn
        startUploadMutex[i].lock();
        startUploadMutex[(i + handler.nTextures - 1) % handler.nTextures].unlock();

        // wait for all previous openGL commands end
        glWaitSync(endUpload[i], 0, GL_TIMEOUT_IGNORED);
        CHECK_GL_ERROR();
        glDeleteSync(endUpload[i]);
        CHECK_GL_ERROR();

        // bind specific texture
        glBindTexture(GL_TEXTURE_2D, handler.GPUtextures[i]);
        CHECK_GL_ERROR();

        // draw texture
        glDrawArrays(GL_TRIANGLES, 0, handler.models[0].numTriangles * 3);
        CHECK_GL_ERROR();

        // set an opengl sync object
        startUpload[i] = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
        CHECK_GL_ERROR();

        // end timing of query
        glEndQuery(GL_TIME_ELAPSED);
        CHECK_GL_ERROR();
    }
}

void drawSquare() {
    CHECK_GL_ERROR();
    for (int i = 0; i < handler.nTextures; i++)
    {

        // update uniform matrices
        updateCommonUniforms(i);
        CHECK_GL_ERROR();

        // set a uniform that tells if we use texture
        glUniform1i(handler.useEmissionTexture, 1);
        CHECK_GL_ERROR();

        // begin timing
        glBeginQuery(GL_TIME_ELAPSED, textureQueries[thisFrameIndex++]);
        CHECK_GL_ERROR();

        // bind a texture to the spot and copy data to it from CPU
        glBindTexture(GL_TEXTURE_2D, handler.GPUtextures[i]);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, handler.widths[0], handler.heights[0], GL_RGB, GL_UNSIGNED_BYTE, handler.textures[i]);
        CHECK_GL_ERROR();
   
        // draw texture
        glDrawArrays(GL_TRIANGLES, 0, handler.models[0].numTriangles * 3);
        CHECK_GL_ERROR();

        // end timing of query
        glEndQuery(GL_TIME_ELAPSED);
        CHECK_GL_ERROR();
    }
}

void drawCubesMethod2() {
    glUseProgram(handler.program);

    // set a uniform that tells if we use texture
    glUniform1i(handler.useEmissionTexture, 0);

    // begin timing
    glFlush();
    glBeginQuery(GL_TIME_ELAPSED, vertexQueries[thisFrameIndex++]);
    glFlush();

    // unlock second thread to start copying data to the part of a buffer that was drawn in the last iteration
    secondMethodMutexData[cubeDrawingIndex].unlock();

    // increase index of part of buffer we are drawing
    cubeDrawingIndex = (cubeDrawingIndex + 1) % numberOfCubeSubbuffers;
    // lock part of the buffer that is going to be drawn
    secondMethodMutexData[cubeDrawingIndex].lock();
    
    // unmap part of the buffer to which were data copied in the last iteration
    glUnmapBuffer(GL_ARRAY_BUFFER);
    CHECK_GL_ERROR();
    
    // map new part of the buffer for the other thread copies data
    cubesMappedPointer = (GLfloat*)glMapBufferRange(GL_ARRAY_BUFFER, sizeof(GLfloat) * cubesSize * 8 * ((cubeDrawingIndex - numberOfCubesPreComputed + numberOfCubeSubbuffers) % numberOfCubeSubbuffers), sizeof(GLfloat) * cubesSize * 8, GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT);
    CHECK_GL_ERROR();
    
    // draw some cubes
    glDrawElements(GL_TRIANGLES, 3003, GL_UNSIGNED_INT, (const void*)(handler.models[1].elementBufferObject + cubesSize * cubeDrawingIndex * sizeof(GLuint)));
    CHECK_GL_ERROR();

    // end timing
    glFlush();
    glEndQuery(GL_TIME_ELAPSED);
    glFlush();

    CHECK_GL_ERROR();
}

void drawCubesMethod3() {

    glUseProgram(handler.program);

    // set a uniform that tells if we use texture
    glUniform1i(handler.useEmissionTexture, 0);

    // begin timing
    glFlush();
    glBeginQuery(GL_TIME_ELAPSED, vertexQueries[thisFrameIndex++]);
    glFlush();



    // unlock second thread to start mapping and copying data to the part of a buffer that was drawn in the last iteration
    thidMethodMutex[cubeDrawingIndex].unlock();
    // increase index of part of buffer we are drawing
    cubeDrawingIndex = (cubeDrawingIndex + 1) % numberOfCubeSubbuffers;

    // lock part of the buffer that is going to be drawns
    thidMethodMutex[cubeDrawingIndex].lock();

    // wait for all previous openGL commands end(copying from other thread)
    glWaitSync(thirdMethodSyncUploadEnd[cubeDrawingIndex], 0, GL_TIMEOUT_IGNORED);
    glDeleteSync(thirdMethodSyncUploadEnd[cubeDrawingIndex]);

    // draw some cubes
    glDrawElements(GL_TRIANGLES, 3003, GL_UNSIGNED_INT, (const void*)(handler.models[1].elementBufferObject + cubesSize * cubeDrawingIndex * sizeof(GLuint)));

    // create an openGL sync object for the other thread to recognize when this thread stopped drawing
    thirdMethodSyncUploadStart[cubeDrawingIndex] = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
    
    // end timing
    glFlush();
    glEndQuery(GL_TIME_ELAPSED);
    glFlush();

    CHECK_GL_ERROR();
}

void drawCubes() {

    glUseProgram(handler.program);

    // set a uniform that tells if we use texture
    glUniform1i(handler.useEmissionTexture, 0);

    // begin timing
    glFlush();
    glBeginQuery(GL_TIME_ELAPSED, vertexQueries[thisFrameIndex++]);
    glFlush();

    // update drawing index
    cubeDrawingIndex = (cubeDrawingIndex + 1) % numberOfCubeSubbuffers;

    // set flags for mapping a part of the buffer
    unsigned char flags = GL_MAP_WRITE_BIT;
    if (bufferMethod == 1)
        flags |= GL_MAP_UNSYNCHRONIZED_BIT;
    
    // bind specific buffer that is going to be mapped
    glBindBuffer(GL_ARRAY_BUFFER, handler.models[1].vertexBufferObject);

    // map part of the buffer
    GLfloat* ptr = (GLfloat*)glMapBufferRange(GL_ARRAY_BUFFER, sizeof(GLfloat) * cubesSize * 8 * ((cubeDrawingIndex - numberOfCubesPreComputed + numberOfCubeSubbuffers) % numberOfCubeSubbuffers), sizeof(GLfloat) * cubesSize * 8, flags);
    CHECK_GL_ERROR();
    
    // fill the mapped buffer with cube data
    fillCubeArray(ptr);

    // unmap buffer
    glUnmapBuffer(GL_ARRAY_BUFFER);

    // draw some cubes
    glDrawElements(GL_TRIANGLES, 3003, GL_UNSIGNED_INT, (const void*)(handler.models[1].elementBufferObject + cubesSize * cubeDrawingIndex * sizeof(GLuint)));
    
    // end timing
    glFlush();
    glEndQuery(GL_TIME_ELAPSED);
    glFlush();

    CHECK_GL_ERROR();
}


void drawModels() {
    if (drawTextures)
        if (useAsynchTextures)
            drawSquareAsync();
        else
            drawSquare();
    else
        if (bufferMethod < 2)
            drawCubes();
        else if (bufferMethod == 2)
            drawCubesMethod2();
        else
            drawCubesMethod3();
}

void addModels() {

    // add square
    handler.models[0].numTriangles = 2;
    handler.models[0].meshMaterial.useEmissionTexture = 1;
    handler.models[0].meshMaterial.ambient = glm::vec3(0.1f);
    handler.models[0].meshMaterial.diffuse = glm::vec3(1.0f);
    handler.models[0].meshMaterial.specular = glm::vec3(1.0f);
    handler.models[0].meshMaterial.emission = glm::vec3(0.1f);
    handler.models[0].meshMaterial.shininess = 3.0f;

    glUseProgram(handler.program);
    glGenVertexArrays(1, &handler.models[0].vertexArrayObject);
    glGenBuffers(1, &handler.models[0].vertexBufferObject);

    glBindVertexArray(handler.models[0].vertexArrayObject);

    glBindBuffer(GL_ARRAY_BUFFER, handler.models[0].vertexBufferObject);
    glBufferData(GL_ARRAY_BUFFER, sizeof(squareVertices), squareVertices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(5 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    CHECK_GL_ERROR();


    glGenTextures(1, &handler.models[0].meshMaterial.emissionTexture[0]);
    glBindTexture(GL_TEXTURE_2D, handler.models[0].meshMaterial.emissionTexture[0]); // all upcoming GL_TEXTURE_2D operations now have effect on this texture object
    // set the texture wrapping parameters
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);	// set texture wrapping to GL_REPEAT (default wrapping method)
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    // set texture filtering parameters
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glGenTextures(1, &handler.models[0].meshMaterial.emissionTexture[1]);
    glBindTexture(GL_TEXTURE_2D, handler.models[0].meshMaterial.emissionTexture[1]); // all upcoming GL_TEXTURE_2D operations now have effect on this texture object
    // set the texture wrapping parameters
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);	// set texture wrapping to GL_REPEAT (default wrapping method)
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    // set texture filtering parameters
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // add cubes
    handler.models[1].numTriangles = cubesSize;
    handler.models[1].meshMaterial.useEmissionTexture = 1;
    handler.models[1].meshMaterial.ambient = glm::vec3(0.1f);
    handler.models[1].meshMaterial.diffuse = glm::vec3(1.0f);
    handler.models[1].meshMaterial.specular = glm::vec3(1.0f);
    handler.models[1].meshMaterial.emission = glm::vec3(0.1f);
    handler.models[1].meshMaterial.shininess = 3.0f;

    glUseProgram(handler.program);
    glGenVertexArrays(1, &handler.models[1].vertexArrayObject);
    glGenBuffers(1, &handler.models[1].vertexBufferObject);
    glGenBuffers(1, &handler.models[1].elementBufferObject);

    glBindVertexArray(handler.models[1].vertexArrayObject);

    glBindBuffer(GL_ARRAY_BUFFER, handler.models[1].vertexBufferObject);

    // GL_MAP_PERSISTENT_BIT lets us copy data to the buffer while another thread is drawing from it
    glBufferStorage(GL_ARRAY_BUFFER, sizeof(GLfloat) * cubesSize * 8 * numberOfCubeSubbuffers, NULL, GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT);

    GLfloat* pointer = (GLfloat*)glMapBufferRange(GL_ARRAY_BUFFER, 0, sizeof(GLfloat) * cubesSize * 8 * numberOfCubesPreComputed, GL_MAP_WRITE_BIT);

    for (size_t i = 0; i < numberOfCubesPreComputed; i++)
    {
        fillCubeArray(pointer);
        pointer += cubesSize * 8;
    }
    glUnmapBuffer(GL_ARRAY_BUFFER);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(5 * sizeof(float)));
    glEnableVertexAttribArray(1);

    // setup indices
    cubesIndices = new GLuint[cubesSize * numberOfCubeSubbuffers];

    for (size_t i = 0; i < cubesSize * numberOfCubeSubbuffers; i++)
    {
        cubesIndices[i] = i;
    }

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, handler.models[1].elementBufferObject);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, numberOfCubeSubbuffers * cubesSize * sizeof(GLuint), cubesIndices, GL_STATIC_DRAW);
    delete[] cubesIndices;

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    CHECK_GL_ERROR();
}

void fillCubeArray(GLfloat* newCubes) {
    for (size_t z = 0; z < nCubesDepth; z++)
    {
        for (size_t y = 0; y < nCubesRow; y++)
        {
            for (size_t x = 0; x < nCubesCol; x++)
            {
                for (size_t i = 0; i < nCubeTriangles / 3; i++)
                {

                    glm::vec3 norm;
                    glm::vec3 first(cubeVertices[3 * i], cubeVertices[3 * i + 1], cubeVertices[3 * i + 2]);
                    glm::vec3 second(cubeVertices[3 * i + 3], cubeVertices[3 * i + 1 + 3], cubeVertices[3 * i + 2 + 3]);
                    glm::vec3 third(cubeVertices[3 * i + 6], cubeVertices[3 * i + 1 + 6], cubeVertices[3 * i + 2 + 6]);
                    glm::vec3 A = second - first; // edge 0 
                    glm::vec3 B = third - first; // edge 1 
                    norm = cross(A, B); // this is the triangle's normal 
                    normalize(norm);
                    for (size_t j = 0; j < 3; j++)
                    {
                        newCubes[(i * 3 + j + (x + y * nCubesCol + z * nCubesCol * nCubesRow) * nCubeTriangles) * 8] = cubeVertices[3 * (i * 3 + j)] + diff * x;            // pos x
                        newCubes[(i * 3 + j + (x + y * nCubesCol + z * nCubesCol * nCubesRow) * nCubeTriangles) * 8 + 1] = cubeVertices[3 * (i * 3 + j) + 1] + diff * y;    // pos y
                        newCubes[(i * 3 + j + (x + y * nCubesCol + z * nCubesCol * nCubesRow) * nCubeTriangles) * 8 + 2] = cubeVertices[3 * (i * 3 + j) + 2] + diff * z;    // pos z
                        newCubes[(i * 3 + j + (x + y * nCubesCol + z * nCubesCol * nCubesRow) * nCubeTriangles) * 8 + 3] = 0;                // tex u
                        newCubes[(i * 3 + j + (x + y * nCubesCol + z * nCubesCol * nCubesRow) * nCubeTriangles) * 8 + 4] = 0;                // tex v

                        newCubes[(i * 3 + j + (x + y * nCubesCol + z * nCubesCol * nCubesRow) * nCubeTriangles) * 8 + 5] = norm.x;                // norm x
                        newCubes[(i * 3 + j + (x + y * nCubesCol + z * nCubesCol * nCubesRow) * nCubeTriangles) * 8 + 6] = norm.y;                // norm y
                        newCubes[(i * 3 + j + (x + y * nCubesCol + z * nCubesCol * nCubesRow) * nCubeTriangles) * 8 + 7] = norm.z;                // norm z
                    }
                }
            }
        }
    }
}

void initializeApplication() {

    // load textures to ram
    int tmp;
    handler.textures[0] = stbi_load("tex1.jpg", &(handler.widths[0]), &(handler.heights[0]), &tmp, 0);
    handler.textures[1] = stbi_load("tex2.jpg", &(handler.widths[1]), &(handler.heights[1]), &tmp, 0);
    handler.textures[2] = stbi_load("tex3.jpg", &(handler.widths[2]), &(handler.heights[2]), &tmp, 0);
    handler.textures[3] = stbi_load("tex4.jpg", &(handler.widths[3]), &(handler.heights[3]), &tmp, 0);
    handler.textures[4] = stbi_load("tex5.jpg", &(handler.widths[4]), &(handler.heights[4]), &tmp, 0);
    handler.textures[5] = stbi_load("tex6.jpg", &(handler.widths[5]), &(handler.heights[5]), &tmp, 0);

    // generate textures in GPU
    glGenTextures(handler.nTextures, handler.GPUtextures);
    for (size_t i = 0; i < handler.nTextures; i++)
    {
        glBindTexture(GL_TEXTURE_2D, handler.GPUtextures[i]);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);	// set texture wrapping to GL_REPEAT (default wrapping method)
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        // set texture filtering parameters
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, handler.widths[0], handler.heights[0], 0, GL_RGB, GL_UNSIGNED_BYTE, handler.textures[3]);
    }

    // create shaders
    GLuint shaders[] = {
            pgr::createShaderFromFile(GL_VERTEX_SHADER, "texvs.glsl"),
            pgr::createShaderFromFile(GL_FRAGMENT_SHADER,"texfs.glsl"),
            0,
    };

    // create shader program
    handler.program = pgr::createProgram(shaders);
    // create uniform variables
    handler.pvmMatrix = glGetUniformLocation(handler.program, "pvmMatrix");
    handler.mMatrix = glGetUniformLocation(handler.program, "mMatrix");
    handler.vMatrix = glGetUniformLocation(handler.program, "vMatrix");
    handler.vmMatrix = glGetUniformLocation(handler.program, "vmMatrix");
    handler.nMatrix = glGetUniformLocation(handler.program, "nMatrix");
    handler.useEmissionTexture = glGetUniformLocation(handler.program, "useEmissionTexture");

    handler.position = glGetAttribLocation(handler.program, "position");
    handler.normal = glGetAttribLocation(handler.program, "normal");


    // setup keyboard variables
    handler.keys = new bool[255];
    for (int i = 0; i < 255; ++i) {
        handler.keys[i] = false;
    }

    handler.specKeys = new bool[255];
    for (int i = 0; i < 255; ++i) {
        handler.specKeys[i] = false;
    }

    // setup camera
    cam.setPosition(glm::vec3(-9.0f, 0.9f, 10.0f));
    cam.setDirection(glm::vec3(0.640737712, -0.0359922871, -0.766915500));
    cam.setUpVector(glm::vec3(0, 1, 0));

    glUseProgram(handler.program);

    addModels();

    glClearColor(0, 0, 0, 1.0f);

    glEnable(GL_DEPTH_TEST);
    CHECK_GL_ERROR();
}

static void update_scene(double time) {
    cam.update(time);

}

static void error_callback(int error, const char* description)
{
    fprintf(stderr, "Error: %s\n", description);
}

static void eraseLastBufferMethod(const unsigned char& lastBufferMethod) {
    switch (lastBufferMethod) {
    case 0:
    case 1:
        // Nothing needs to be changed
        break;
    case 2:
        // the buffer is mapped in method 2
        glUnmapBuffer(GL_ARRAY_BUFFER);
        CHECK_GL_ERROR();
        bufferThreadEnd = true;
        // thread that is ending is blocked by this mutex so we need to unlock it
        secondMethodMutexData[cubeDrawingIndex].unlock();
        // end the thread
        bufferThread.join();
        bufferThreadEnd = false;
        break;
    case 3:
        // we need to end the thread
        bufferThreadEnd = true;
        // this unlocks all the mutexes that block the the thread
        drawCubesMethod3();
        // end the thread
        bufferThread.join();
        bufferThreadEnd = false;
        // this mutex is locked by ended thread
        thidMethodMutex[cubeDrawingIndex].unlock();
        break;
    }
}

void secondMethodThread();
void thirdMethodThread();

static void startBufferMethod(const unsigned char& newBufferMethod) {
    switch (newBufferMethod) {
    case 0:
    case 1:
        // Nothing needs to be changed
        break;
    case 2:
        // map buffer and start thread
        glBindVertexArray(handler.models[1].vertexArrayObject);
        glBindBuffer(GL_ARRAY_BUFFER, handler.models[1].vertexBufferObject);
        // map first buffer so the second thread can start copying data
        cubesMappedPointer = (GLfloat*)glMapBufferRange(GL_ARRAY_BUFFER, 0, sizeof(GLfloat) * cubesSize * 8, GL_MAP_WRITE_BIT | GL_MAP_UNSYNCHRONIZED_BIT);

        secondMethodMutexData[numberOfCubeSubbuffers - 1].lock();
        bufferThread = std::thread(secondMethodThread);
        break;
    case 3:
        thidMethodMutex[numberOfCubeSubbuffers - 1].lock();
        // set init sync objects(wait/delete would call an error if the object does not exist)
        for (size_t i = 0; i < numberOfCubeSubbuffers; i++)
        {
            thirdMethodSyncUploadStart[i] = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
            thirdMethodSyncUploadEnd[i] = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);

        }

        // start thread
        bufferThread = std::thread(thirdMethodThread);
        break;
    }
    cubeDrawingIndex = numberOfCubeSubbuffers - 1;
}

static void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    // change buffer method
    if ((key >= '0') && key <= '3' && action == GLFW_RELEASE) {
        changeMethod = true;
        newMethod = key - '0';
    }

    // change texture method
    if ((key == 'T' || key == 't') && action == GLFW_RELEASE) {
        changeTexture = true;
    }

    // change between cubes and squares
    if ((key == 'q' || key == 'Q') && action == GLFW_RELEASE) {
        drawTextures = !drawTextures;
        thisFrameIndex = 0;
        glBindVertexArray(handler.models[!drawTextures].vertexArrayObject);
    }


    if (key >= 'A' && key <= 'Z')
        key -= 'A' - 'a';

    if (key >= 'a' && key <= 'z')
        handler.keys[key] = action == GLFW_PRESS || action == GLFW_REPEAT;

    // exit app
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
        glfwSetWindowShouldClose(window, GLFW_TRUE);

    // move faster
    if (key == GLFW_KEY_LEFT_SHIFT) {
        handler.specKeys[keys::shift] = action == GLFW_PRESS || action == GLFW_REPEAT;
    }
}

void copyDataToPBO(unsigned int index) {
    // binding pbo for the first data transfer
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pbo[curPBO]); //bind pbo

    // map the pbo 
    GLubyte* ptr = (GLubyte*)glMapBufferRange(GL_PIXEL_UNPACK_BUFFER, 0, handler.widths[0] * handler.heights[0] * sizeof(unsigned char) * 3, GL_MAP_WRITE_BIT);

    // copy
    memcpy(ptr, handler.textures[index], handler.widths[0] * handler.heights[0] * sizeof(unsigned char) * 3);
    glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);
    CHECK_GL_ERROR();
}

void getDataFromPBOToTexture(unsigned int index) {
    // bind specific PBO
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pbo[1 - curPBO]); 

    // get data from bound buffer to the texture
    glTextureSubImage2D(handler.GPUtextures[index], 0, 0, 0, handler.widths[0], handler.heights[0], GL_RGB, GL_UNSIGNED_BYTE, (void*)(0));
    CHECK_GL_ERROR();
}

void generateNewPBO(unsigned int index) {

    glGenBuffers(1, &pbo[index]);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pbo[index]); //bind pbo
    glBufferStorage(GL_PIXEL_UNPACK_BUFFER, handler.widths[0] * handler.heights[0] * sizeof(unsigned char) * 3, NULL, GL_MAP_WRITE_BIT);
    CHECK_GL_ERROR();
}

void textureAsyncThread() {
    // set context
    glfwMakeContextCurrent(handler.textureContextWindow);

    // lock first texture, because we have to preload textures and this blocks the other thread
    startUploadMutex[0].lock();

    // creating first pbo
    generateNewPBO(0);

    // creating second pbo
    generateNewPBO(1);

    // copy first texture data to the GPU
    copyDataToPBO(0);
    curPBO = 1 - curPBO;

    int i = 0;
    for (; i < preloadedTextures; i++)
    {
        // copy data from CPU to PBO on GPU
        copyDataToPBO((i + 1) % handler.nTextures);

        // get data from the pbo, we filled with data in last iteration, to texture
        getDataFromPBOToTexture(i);

        // update PBO index
        curPBO = 1 - curPBO;
    }

    // unlock mutex that kept drawing thread from drawing unprepared textures
    startUploadMutex[0].unlock();
    
    // this mutex will be unlocked in the beginning of the next the cycle
    startUploadMutex[(i + handler.nTextures - 1) % handler.nTextures].lock();

    while (!end && !endTextureMethod) {

        // lock the texture that we want to copy data into
        startUploadMutex[i].lock();
        // unlock last texture that we already finished copying data into
        startUploadMutex[(i + handler.nTextures - 1) % handler.nTextures].unlock();

        // this prevents first textures to be drawn before they are loaded
        firstTextureLoaded = true;  

        // wait for the drawing of the texture, we are going to change, to end
        glWaitSync(startUpload[i], 0, GL_TIMEOUT_IGNORED);
        glDeleteSync(startUpload[i]);

        // copying data of a texture to pbo
        copyDataToPBO((i + 1) % handler.nTextures);
       
        // copying from PBO, we copied data to in last iteration, to texture
        getDataFromPBOToTexture(i);

        // setup the openGL sync object, for the other thread to know when the copying of data to textures is done
        endUpload[i] = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
        CHECK_GL_ERROR();

        // update indices
        i = (i + 1) % handler.nTextures;
        curPBO = 1 - curPBO;

    }
    // unlock the lock, which is locked by last iteration
    startUploadMutex[(i + handler.nTextures - 1) % handler.nTextures].unlock();

    glfwMakeContextCurrent(NULL);
}

void update(double &lastTime) {
    double currentTime = glfwGetTime();
        // setup camera
        double oldMouseX = handler.mouseX, oldMouseY = handler.mouseY;
        glfwGetCursorPos(handler.window, &(handler.mouseX), &(handler.mouseY));
        handler.mouseDx = -handler.mouseX + oldMouseX;
        handler.mouseDy = -handler.mouseY + oldMouseY;

        secondMethodMutexCamera.lock();
        update_scene(currentTime - lastTime);
        lastTime = currentTime;
        secondMethodMutexCamera.unlock();
}

void secondMethodThread() {
    // this mutex is going to be unlocked in the beginning of the first iteration of the following cycle
    secondMethodMutexData[numberOfCubeSubbuffers - numberOfCubesPreComputed].lock();

    // CPU time to be used by update method(move of camera is dependent on delta time 
    double lastTime = glfwGetTime();

    unsigned int index = numberOfCubeSubbuffers - numberOfCubesPreComputed;
    while (!end && !bufferThreadEnd) {
        // update scene
        update(lastTime);
        
        // send data to mapped part of a buffer	   	
        fillCubeArray(cubesMappedPointer);

        // tell the other thread that the data from this index has been copied
        secondMethodMutexData[index].unlock();

        //update index
        index = (index + 1) % numberOfCubeSubbuffers;

        // tell the other thread that the data from this index is about to be change and therfore the other thread cannot use this data
        secondMethodMutexData[index].lock();
    }

    // unlock the mutex that was locked in the last iteration
    secondMethodMutexData[index].unlock();
}

void thirdMethodThread() {

    glfwMakeContextCurrent(handler.bufferContextWindow);

    // this mutex is going to be unlocked in the end of the first iteration of the following cycle. 
    // prevents the other thread from drawing from the part of buffer that is currently mapped in this thread
    thidMethodMutex[numberOfCubeSubbuffers - numberOfCubesPreComputed].lock();

    // CPU time to be used by update method(move of camera is dependent on delta time 
    double lastTime = glfwGetTime();

    unsigned int index = numberOfCubeSubbuffers - numberOfCubesPreComputed;
    while (!end && !bufferThreadEnd) {
        // update scene
        update(lastTime);

        // wait for the drawing, of the part of buffer we are going to change, to end
        glWaitSync(thirdMethodSyncUploadStart[index], 0, GL_TIMEOUT_IGNORED);
        glDeleteSync(thirdMethodSyncUploadStart[index]);

        // bind buffer that we are going to map and copy data to
        glBindBuffer(GL_ARRAY_BUFFER, handler.models[1].vertexBufferObject);

        // map a part of buffer that we are going to fill with new data
        cubesMappedPointer = (GLfloat*)glMapBufferRange(GL_ARRAY_BUFFER, sizeof(GLfloat) * cubesSize * 8 * index, sizeof(GLfloat) * cubesSize * 8, GL_MAP_WRITE_BIT | GL_MAP_UNSYNCHRONIZED_BIT);
        
        // fill the mapped the part of buffer with cube data
        fillCubeArray(cubesMappedPointer);

        // unmap the part of buffer we were using
        if (glUnmapBuffer(GL_ARRAY_BUFFER) != GL_TRUE) {
            std::cout << "ERROR\n";
        }

        // setup the openGL sync object, for the other thread to know when the copying of data to current part of buffer is done
        thirdMethodSyncUploadEnd[index] = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
        CHECK_GL_ERROR();

        // tell the other thread that part of buffer with index "index" is unmapped
        thidMethodMutex[index].unlock();

        index = (index + 1) % numberOfCubeSubbuffers;
        // tell the other thread that it cannot draw from part of the buffer, because we are going to copy new data into it
        thidMethodMutex[index].lock();

    }

    // This mutex was locked in last iteration of previous cycle
    thidMethodMutex[index].unlock();

    glfwMakeContextCurrent(NULL);
}

// this method setup mutexes and locks for future usage of all methods
void setupLocks() {
    for (size_t i = 0; i < numberOfCubeSubbuffers; i++)
    {
        thirdMethodSyncUploadStart[i] = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
        thirdMethodSyncUploadEnd[i] = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);

    }


    for (size_t i = 0; i < preloadedTextures; i++)
    {
        endUpload[i] = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
    }

    for (size_t i = preloadedTextures; i < handler.nTextures; i++)
    {
        startUpload[i] = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
    }

    startUpload[preloadedTextures] = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);

    startUploadMutex[handler.nTextures - 1].lock();

}

int main(int argc, char* argv[]) {
    glfwSetErrorCallback(error_callback);

    //ilInit(); UNCOMMENT IF DEVILL NEEDED

    if (!glfwInit())
        exit(EXIT_FAILURE);

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);

    handler.window = glfwCreateWindow(640, 480, "Simple example", NULL, NULL);

    if (!handler.window)
    {
        glfwTerminate();
        exit(EXIT_FAILURE);
    }


    glfwSetKeyCallback(handler.window, key_callback);

    glfwMakeContextCurrent(handler.window);


    glfwGetCursorPos(handler.window, &(handler.mouseX), &(handler.mouseY));

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        std::cout << "Failed to initialize GLAD" << std::endl;
        return -1;
    }

    initializeApplication();

    glGenQueries(maxCounter * handler.nTextures, textureQueries);
    glGenQueries(maxCounter, vertexQueries);

    glfwSetInputMode(handler.window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    if (glfwRawMouseMotionSupported())
        glfwSetInputMode(handler.window, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);

    glfwSwapInterval(1);

    // create a context for second thread in async vertex data transfer(method 2 and 3)
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
    handler.bufferContextWindow = glfwCreateWindow(640, 480, "Second Window", NULL, handler.window);

    // create a context for second thread in async texture data transfer
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
    handler.textureContextWindow = glfwCreateWindow(640, 480, "Second Window", NULL, handler.window);

    // prepare for calculating time on CPU
    double lastTime = glfwGetTime();
    unsigned int counter = 0;
    double averageTimePerFrame = 0;

    // this method setup mutexes and locks for future usage of all methods
    setupLocks();    

    glBindVertexArray(handler.models[0].vertexArrayObject);

    while (!glfwWindowShouldClose(handler.window)) {

        // collect info about draw time of current method
        const double thisTime = glfwGetTime();
        double diff = thisTime - lastTime;
        lastTime = thisTime;
        unsigned int tmp;
        if (drawTextures)
            tmp = maxCounter * handler.nTextures;
        else
            tmp = maxCounter;
        if (thisFrameIndex >= tmp) {
            if (drawTextures) {
                for (size_t i = 0; i < maxCounter * handler.nTextures; i++)
                {
                    GLuint64 timer;
                    glGetQueryObjectui64v(textureQueries[i],
                        GL_QUERY_RESULT, &timer);
                    averageTimePerFrame += timer * 0.000000001;
                }
            }
            else {
                for (size_t i = 0; i < maxCounter; i++)
                {
                    GLuint64 timer;
                    glGetQueryObjectui64v(vertexQueries[i],
                        GL_QUERY_RESULT, &timer);
                    averageTimePerFrame += timer * 0.000000001;
                }
            }

            averageTimePerFrame /= tmp;
            std::cout << averageTimePerFrame << " s" << std::endl;
            averageTimePerFrame = 0;
            counter = 0;
            thisFrameIndex = 0;
        }

        // if the usare changed vertex transfer method
        if (changeMethod && newMethod != bufferMethod) {
            changeMethod = false;
            if (!drawTextures) {
                std::cout << "buffer method changed to " << (unsigned int)newMethod << std::endl;
                thisFrameIndex = 0;
                // clean after the last method
                eraseLastBufferMethod(bufferMethod);
                // start new method
                startBufferMethod(newMethod);
                bufferMethod = newMethod;
            }
        }

        // update scene for vertex methods without second thread
        if (bufferMethod == 0 || bufferMethod == 1) {
            double oldMouseX = handler.mouseX, oldMouseY = handler.mouseY;
            glfwGetCursorPos(handler.window, &(handler.mouseX), &(handler.mouseY));
            handler.mouseDx = -handler.mouseX + oldMouseX;
            handler.mouseDy = -handler.mouseY + oldMouseY;
            update_scene(diff);
        }

        // if the usare wants to change the texture transfer method
        if (changeTexture ) {
            changeTexture = false;
            if (drawTextures) {
                changeTexture = false;
                useAsynchTextures = !useAsynchTextures;
                if (useAsynchTextures) {
                    std::cout << "using async texture load" << std::endl;
                    endTextureMethod = false;
                    thisFrameIndex = 0;
                    // delete previous thread
                    if (textureThreadWasStarted) 
                        textureThread.join();
                    // setup starting fences(some of them might be used before they are set in second thread because first textures are used from previous loads.
                    for (size_t i = 0; i < preloadedTextures; i++)
                    {
                        endUpload[i] = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
                    }
                    // create the second thread for async texture trasfer
                    textureThread = std::thread(textureAsyncThread);
                    textureThreadWasStarted = true;
                }
                else {
                    std::cout << "using sync texture load" << std::endl;
                    endTextureMethod = true;
                }
            }
        }

        glfwGetFramebufferSize(handler.window, &handler.windowWidth, &handler.windowHeight);
        glViewport(0, 0, handler.windowWidth, handler.windowHeight);

        glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glUseProgram(handler.program);

        drawModels();

        glfwSwapBuffers(handler.window);
        glfwPollEvents();

        CHECK_GL_ERROR();

    }

    end = true;


    // unlock mutexes from the third vertex trasfer method
    const unsigned int index = (cubeDrawingIndex + numberOfCubeSubbuffers - numberOfCubesPreComputed) % numberOfCubeSubbuffers;    
    for (size_t i = 0; i < numberOfCubeSubbuffers; i++)
    {
        if(i != index)
            thidMethodMutex[i].unlock();
    }

    // if the second or third buffer transfer method was used, we need to end their thread
    if (bufferMethod == 3 || bufferMethod == 2) {
        bufferThread.join();
    }

    // if the async texture trasfer method was used, we need to end its thread
    if (textureThreadWasStarted) textureThread.join();

    // delete openGL query objects
    glDeleteQueries(handler.nTextures* maxCounter, textureQueries);
    glDeleteQueries(maxCounter, vertexQueries);

    // delete alocated memory
    delete[] handler.keys;
    delete[] handler.specKeys;

    // destroy contexts
    glfwDestroyWindow(handler.window);
    glfwDestroyWindow(handler.bufferContextWindow);
    glfwDestroyWindow(handler.textureContextWindow);

    // end the application
    glfwTerminate();
    exit(EXIT_SUCCESS);
}