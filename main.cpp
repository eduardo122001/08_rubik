#define GLAD_GL_IMPLEMENTATION
#include <glad/gl.h>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include "myglm.h"

#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <cstdlib>

#define WIDTH 1000
#define HEIGHT 800

class Scene;
class RubikCube;

Scene* g_SceneInstance = nullptr;
RubikCube* g_RubikInstance = nullptr;

// Track the held state of arrow keys
bool g_LeftPressed  = false;
bool g_RightPressed = false;
bool g_UpPressed    = false;
bool g_DownPressed  = false;

// Helper function to read, compile, and link external shaders
unsigned int loadShaders(const char* vertexPath, const char* fragmentPath) {
    std::string vertexCode;
    std::string fragmentCode;
    std::ifstream vShaderFile;
    std::ifstream fShaderFile;

    // Ensure ifstream objects can throw exceptions
    vShaderFile.exceptions(std::ifstream::failbit | std::ifstream::badbit);
    fShaderFile.exceptions(std::ifstream::failbit | std::ifstream::badbit);

    try {
        // Open files
        vShaderFile.open(vertexPath);
        fShaderFile.open(fragmentPath);
        std::stringstream vShaderStream, fShaderStream;
        
        // Read file's buffer contents into streams
        vShaderStream << vShaderFile.rdbuf();
        fShaderStream << fShaderFile.rdbuf();
        
        // Close file handlers
        vShaderFile.close();
        fShaderFile.close();
        
        // Convert stream into string
        vertexCode = vShaderStream.str();
        fragmentCode = fShaderStream.str();
    }
    catch (std::ifstream::failure& e) {
        std::cerr << "ERROR::SHADER::FILE_NOT_SUCCESSFULLY_READ: " << e.what() << std::endl;
    }

    const char* vShaderCode = vertexCode.c_str();
    const char* fShaderCode = fragmentCode.c_str();

    int success;
    char infoLog[512];

    // Vertex Shader
    unsigned int vertex = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertex, 1, &vShaderCode, NULL);
    glCompileShader(vertex);
    glGetShaderiv(vertex, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(vertex, 512, NULL, infoLog);
        std::cerr << "ERROR::SHADER::VERTEX::COMPILATION_FAILED\n" << infoLog << std::endl;
    }

    // Fragment Shader
    unsigned int fragment = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragment, 1, &fShaderCode, NULL);
    glCompileShader(fragment);
    glGetShaderiv(fragment, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(fragment, 512, NULL, infoLog);
        std::cerr << "ERROR::SHADER::FRAGMENT::COMPILATION_FAILED\n" << infoLog << std::endl;
    }

    // Shader Program Linking
    unsigned int programID = glCreateProgram();
    glAttachShader(programID, vertex);
    glAttachShader(programID, fragment);
    glLinkProgram(programID);
    glGetProgramiv(programID, GL_LINK_STATUS, &success);
    if (!success) {
        glGetProgramInfoLog(programID, 512, NULL, infoLog);
        std::cerr << "ERROR::SHADER::PROGRAM::LINKING_FAILED\n" << infoLog << std::endl;
    }

    // Delete the shaders as they're linked into our program now and no longer necessary
    glDeleteShader(vertex);
    glDeleteShader(fragment);

    return programID;
}

class Shape {
public:
    unsigned int VAO, VBO, EBO, EBOedges;
    unsigned int lastColorLoc = 0, lastModelLoc = 0;
    unsigned int lastShaderProgram = 0;

    bool isVisible = true;

    // The single source of truth for the object's spatial transformation
    myglm::mat4 modelMatrix = myglm::mat4(1.0f);

    std::vector<myglm::vec3> vertices;
    std::vector<unsigned int> indices;
    std::vector<unsigned int> indicesEdges;

    void swapVisibility() { 
        isVisible = !isVisible; 
    }

    void draw(unsigned int shaderProgram) {
        if (!isVisible) return;

        // Cache uniform locations only when switching shaders
        if (lastShaderProgram != shaderProgram) {
            glUseProgram(shaderProgram);
            lastShaderProgram = shaderProgram;
            lastModelLoc = glGetUniformLocation(shaderProgram, "model");
            lastColorLoc = glGetUniformLocation(shaderProgram, "color");
        }

        // Pass this object's specific spatial matrix layout to the shader
        glUniformMatrix4fv(lastModelLoc, 1, GL_FALSE, myglm::value_ptr(modelMatrix));
        
        glBindVertexArray(VAO);
        drawShape();
        drawEdges();
        drawVertices();
        glBindVertexArray(0);
    }

    void drawVertices() {
        myglm::vec4 color(0.0f, 0.0f, 0.0f, 1.0f);
        glUniform4fv(lastColorLoc, 1, myglm::value_ptr(color));
        glDrawArrays(GL_POINTS, 0, static_cast<GLsizei>(vertices.size()));
    }

    // Pure Virtual Interfaces to be implemented by child geometries
    virtual void drawEdges() = 0;
    virtual void drawShape() = 0;
    virtual void generateVertices(myglm::vec3 position, myglm::vec3 scale, myglm::vec3 rotation) = 0;
    virtual void generateIndices() = 0;
    virtual void generateIndicesEdges() = 0;

    void initialize(myglm::vec3 position, myglm::vec3 scale, myglm::vec3 rotation) {
        generateVertices(position, scale, rotation);
        generateIndices();
        generateIndicesEdges();

        glGenVertexArrays(1, &VAO);
        glGenBuffers(1, &VBO);
        glGenBuffers(1, &EBO);
        glGenBuffers(1, &EBOedges);

        glBindVertexArray(VAO);

        // Upload vertex coordinate data
        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(myglm::vec3) * vertices.size(), vertices.data(), GL_STATIC_DRAW);

        // Upload fill triangle indices
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(unsigned int) * indices.size(), indices.data(), GL_STATIC_DRAW);

        // Upload line border indices
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBOedges);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(unsigned int) * indicesEdges.size(), indicesEdges.data(), GL_STATIC_DRAW);

        // Set attribute layout pointers (Location 0 matching shader input)
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(myglm::vec3), (void*)0);
        glEnableVertexAttribArray(0);

        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindVertexArray(0);
    }

	void translate(const myglm::vec3& offset) {
		// Performs a local translation by default
		modelMatrix = myglm::translate(modelMatrix, offset);
	}

	void scale(const myglm::vec3& factor) {
		modelMatrix = myglm::scale(modelMatrix, factor);
	}

	void rotateLocal(float angleInRadians, const myglm::vec3& axis) {
		modelMatrix = myglm::rotate(modelMatrix, angleInRadians, axis);
	}

	void rotateGlobal(float angleInRadians, const myglm::vec3& axis) {
		myglm::mat4 rot = myglm::rotate(myglm::mat4(1.0f), angleInRadians, axis);
		modelMatrix = rot * modelMatrix;
	}

	void displayState() {
		// 1. Extract Position (4th column offsets)
		float posX = modelMatrix.m[3][0];
		float posY = modelMatrix.m[3][1];
		float posZ = modelMatrix.m[3][2];

		// 2. Extract Scale (Magnitude/Length of column vectors)
		float scaleX = std::sqrt(modelMatrix.m[0][0] * modelMatrix.m[0][0] + 
								modelMatrix.m[0][1] * modelMatrix.m[0][1] + 
								modelMatrix.m[0][2] * modelMatrix.m[0][2]);
								
		float scaleY = std::sqrt(modelMatrix.m[1][0] * modelMatrix.m[1][0] + 
								modelMatrix.m[1][1] * modelMatrix.m[1][1] + 
								modelMatrix.m[1][2] * modelMatrix.m[1][2]);
								
		float scaleZ = std::sqrt(modelMatrix.m[2][0] * modelMatrix.m[2][0] + 
								modelMatrix.m[2][1] * modelMatrix.m[2][1] + 
								modelMatrix.m[2][2] * modelMatrix.m[2][2]);

		std::cout << "--- Shape Spatial State ---\n"
				<< "Position: [" << posX << ", " << posY << ", " << posZ << "]\n"
				<< "Scale:    [" << scaleX << ", " << scaleY << ", " << scaleZ << "]\n"
				<< "---------------------------\n\n";
	}

    virtual ~Shape() {
        glDeleteVertexArrays(1, &VAO);
        glDeleteBuffers(1, &VBO);
        glDeleteBuffers(1, &EBO);
        glDeleteBuffers(1, &EBOedges);
    }
};

class Cubie : public Shape {
private:
    myglm::vec4 faceColors[6];

public:
    // 1. Discrete Integer Coordinates (-1, 0, 1) - The absolute source of truth
    int gridX, gridY, gridZ;

    // 2. The baseline perfect position matrix before temporary animation offsets are applied
    myglm::mat4 homeMatrix = myglm::mat4(1.0f);

    // Is this specific piece currently undergoing an active layer spin?
    bool isAnimating = false;

    Cubie(myglm::vec3 position, myglm::vec3 scale, myglm::vec3 rotation, const std::vector<myglm::vec4>& colorsArray) {
        for (int f = 0; f < 6; f++) {
            faceColors[f] = colorsArray[f];
        }
        initialize(position, scale, rotation);
        
        // Save the clean initial setup matrix as the baseline home configuration
        homeMatrix = modelMatrix;
    }

    void generateVertices(myglm::vec3 position, myglm::vec3 scaleFactor, myglm::vec3 rotationAngles) override {
        vertices = {
            myglm::vec3(-0.5f, -0.5f, -0.5f), myglm::vec3(-0.5f, -0.5f,  0.5f),
            myglm::vec3( 0.5f, -0.5f, -0.5f), myglm::vec3( 0.5f, -0.5f,  0.5f),
            myglm::vec3(-0.5f,  0.5f, -0.5f), myglm::vec3(-0.5f,  0.5f,  0.5f),
            myglm::vec3( 0.5f,  0.5f, -0.5f), myglm::vec3( 0.5f,  0.5f,  0.5f)
        };

        modelMatrix = myglm::mat4(1.0f);
        modelMatrix = myglm::translate(modelMatrix, position);
        
        if (rotationAngles.x != 0.0f) modelMatrix = myglm::rotate(modelMatrix, rotationAngles.x, myglm::vec3(1.0f, 0.0f, 0.0f));
        if (rotationAngles.y != 0.0f) modelMatrix = myglm::rotate(modelMatrix, rotationAngles.y, myglm::vec3(0.0f, 1.0f, 0.0f));
        if (rotationAngles.z != 0.0f) modelMatrix = myglm::rotate(modelMatrix, rotationAngles.z, myglm::vec3(0.0f, 0.0f, 1.0f));
        
        modelMatrix = myglm::scale(modelMatrix, scaleFactor);
    }

    void generateIndices() override {
        indices = {
            0, 2, 6,  0, 6, 4,  1, 5, 7,  1, 7, 3,
            1, 0, 4,  1, 4, 5,  2, 3, 7,  2, 7, 6,
            4, 6, 7,  4, 7, 5,  1, 3, 2,  1, 2, 0  
        };
    }

    void generateIndicesEdges() override {
        indicesEdges = {
            0, 1,  1, 3,  3, 2,  2, 0,
            4, 5,  5, 7,  7, 6,  6, 4,
            0, 4,  1, 5,  2, 6,  3, 7  
        };
    }

    // INTERCEPT AND OVERRIDE STANDARD DRAW ROUTINE
    // This passes an interpolated matrix during animations without mutating the baseline positions
    void draw(unsigned int shaderProgram, const myglm::mat4& activeAnimRotation) {
        if (!isVisible) return;

        if (lastShaderProgram != shaderProgram) {
            glUseProgram(shaderProgram);
            lastShaderProgram = shaderProgram;
            lastModelLoc = glGetUniformLocation(shaderProgram, "model");
            lastColorLoc = glGetUniformLocation(shaderProgram, "color");
        }

        // Compute localized dynamic layout composition 
        myglm::mat4 visualMatrix = homeMatrix;
        if (isAnimating) {
            visualMatrix = activeAnimRotation * homeMatrix;
        }

        glUniformMatrix4fv(lastModelLoc, 1, GL_FALSE, myglm::value_ptr(visualMatrix));
        
        glBindVertexArray(VAO);
        drawShape();
        drawEdges();
        drawVertices();
        glBindVertexArray(0);
    }

    void drawShape() override {
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
        for (int f = 0; f < 6; f++) {
            glUniform4fv(lastColorLoc, 1, myglm::value_ptr(faceColors[f]));
            glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, reinterpret_cast<void*>(6 * f * sizeof(unsigned int)));
        }
    }

    void drawEdges() override {
        myglm::vec4 wireColor(1.0f, 1.0f, 1.0f, 1.0f);
        glUniform4fv(lastColorLoc, 1, myglm::value_ptr(wireColor));
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBOedges);
        glDrawElements(GL_LINES, 24, GL_UNSIGNED_INT, nullptr);
    }
};

class RubikCube {
public:
    std::vector<Cubie*> cubies;

    // The 9 Layer Rotation Batches
    std::vector<Cubie*> layerTop;    std::vector<Cubie*> layerMiddleY; std::vector<Cubie*> layerBottom;
    std::vector<Cubie*> layerLeft;   std::vector<Cubie*> layerMiddleX; std::vector<Cubie*> layerRight;
    std::vector<Cubie*> layerFront;  std::vector<Cubie*> layerMiddleZ; std::vector<Cubie*> layerBack;

    float sizeSingleCubie;

    // --- Animation State Engine Data ---
    bool isAnimating = false;
    std::vector<Cubie*>* currentAnimatedBatch = nullptr;
    myglm::vec3 currentAnimationAxis = myglm::vec3(0.0f);
    
    float animationProgressAngle = 0.0f;
    float animationTargetAngle = 0.0f;
    float rotationSpeed = 300.0f; // Velocity measured in degrees per second

    RubikCube(myglm::vec3 centerPosition, float totalSize) {
        sizeSingleCubie = totalSize / 3.0f;
        myglm::vec3 cubieScale(sizeSingleCubie, sizeSingleCubie, sizeSingleCubie);
        myglm::vec3 initialRotation(0.0f, 0.0f, 0.0f);

        std::vector<myglm::vec4> palette = {
            myglm::vec4(1.0f, 1.0f, 1.0f, 1.0f), myglm::vec4(1.0f, 0.5f, 0.0f, 1.0f),
            myglm::vec4(0.0f, 0.0f, 1.0f, 1.0f), myglm::vec4(1.0f, 0.0f, 0.0f, 1.0f),
            myglm::vec4(0.0f, 1.0f, 0.0f, 1.0f), myglm::vec4(1.0f, 1.0f, 0.0f, 1.0f)
        };
        myglm::vec4 structuralInternalBlack(0.15f, 0.15f, 0.15f, 1.0f);

        for (int i = 0; i < 3; i++) {       // Z-axis (Depth)
            for (int j = 0; j < 3; j++) {   // Y-axis (Height)
                for (int k = 0; k < 3; k++) { // X-axis (Width)

                    myglm::vec3 targetCubiePos(
                        centerPosition.x + (k - 1) * sizeSingleCubie,
                        centerPosition.y + (j - 1) * sizeSingleCubie,
                        centerPosition.z + (i - 1) * sizeSingleCubie
                    );

                    std::vector<myglm::vec4> assignedColors(6, structuralInternalBlack);
                    if (i == 0) assignedColors[0] = palette[4]; // Front
                    if (i == 2) assignedColors[1] = palette[2]; // Back
                    if (k == 0) assignedColors[2] = palette[1]; // Left
                    if (k == 2) assignedColors[3] = palette[3]; // Right
                    if (j == 2) assignedColors[4] = palette[0]; // Up
                    if (j == 0) assignedColors[5] = palette[5]; // Down

                    Cubie* newCubie = new Cubie(targetCubiePos, cubieScale, initialRotation, assignedColors);
                    
                    // Assign permanent drift-free logical coordinates
                    newCubie->gridX = k - 1; // -1, 0, 1
                    newCubie->gridY = j - 1; // -1, 0, 1
                    newCubie->gridZ = i - 1; // -1, 0, 1

                    cubies.push_back(newCubie);
                }
            }
        }
        updateTrackingBatches();
    }

    // 1. Input Interface Command: Begins a smooth rotation operation
    void queueLayerRotation(std::vector<Cubie*>& targetBatch, float angleDegrees, const myglm::vec3& axis) {
        if (isAnimating) return; // Action lock out flag guard

        isAnimating = true;
        currentAnimatedBatch = &targetBatch;
        currentAnimationAxis = axis;
        animationProgressAngle = 0.0f;
        animationTargetAngle = angleDegrees;

        // Flags target components to render dynamically
        for (Cubie* cubie : *currentAnimatedBatch) {
            cubie->isAnimating = true;
        }
    }

    // 2. Real-Time Processing Loop
    void updateAnimation(float deltaTime) {
        if (!isAnimating) return;

        // Progress rotation based on absolute timing variables
        float direction = (animationTargetAngle > 0.0f) ? 1.0f : -1.0f;
        animationProgressAngle += direction * rotationSpeed * deltaTime;

        // Bounds Check: Check if target angle has been met
        bool animationFinished = false;
        if (direction > 0.0f && animationProgressAngle >= animationTargetAngle) {
            animationProgressAngle = animationTargetAngle;
            animationFinished = true;
        } else if (direction < 0.0f && animationProgressAngle <= animationTargetAngle) {
            animationProgressAngle = animationTargetAngle;
            animationFinished = true;
        }

        // If animation reaches its destination, bake perfect matrix properties and release logic locks
        if (animationFinished) {
            float finalRad = myglm::radians(animationTargetAngle);
            myglm::mat4 finalRotMatrix = myglm::rotate(myglm::mat4(1.0f), finalRad, currentAnimationAxis);

            for (Cubie* cubie : *currentAnimatedBatch) {
                cubie->isAnimating = false;
                
                // 1. Commit exact 90-degree update into permanent home matrix layout
                cubie->homeMatrix = finalRotMatrix * cubie->homeMatrix;

                // 2. Calculate new integer coordinates using standard discrete rotation matrix logic
                int oldX = cubie->gridX;
                int oldY = cubie->gridY;
                int oldZ = cubie->gridZ;

                if (currentAnimationAxis.y > 0.5f) { // Turning Y-Axis layers (Top / MidY / Bottom)
                    if (animationTargetAngle > 0.0f) { // 90 deg clockwise
                        cubie->gridX = -oldZ; cubie->gridZ = oldX;
                    } else { // -90 deg
                        cubie->gridX = oldZ;  cubie->gridZ = -oldX;
                    }
                }
                else if (currentAnimationAxis.x > 0.5f) { // Turning X-Axis layers (Right / MidX / Left)
                    if (animationTargetAngle > 0.0f) {
                        cubie->gridY = oldZ;  cubie->gridZ = -oldY;
                    } else {
                        cubie->gridY = -oldZ; cubie->gridZ = oldY;
                    }
                }
                else if (currentAnimationAxis.z > 0.5f) { // Turning Z-Axis layers (Front / MidZ / Back)
                    if (animationTargetAngle > 0.0f) {
                        cubie->gridX = oldY;  cubie->gridY = -oldX;
                    } else {
                        cubie->gridX = -oldY; cubie->gridY = oldX;
                    }
                }
            }

            // Sync arrays and clear tracking states
            isAnimating = false;
            currentAnimatedBatch = nullptr;
            animationProgressAngle = 0.0f;
            updateTrackingBatches();
        }
    }

    // Helper mapping tracking view to lookups inside discrete fields
    void updateTrackingBatches() {
        layerTop.clear();    layerMiddleY.clear(); layerBottom.clear();
        layerLeft.clear();   layerMiddleX.clear(); layerRight.clear();
        layerFront.clear();  layerMiddleZ.clear(); layerBack.clear();

        for (Cubie* cubie : cubies) {
            if (cubie->gridY == 1)       layerTop.push_back(cubie);
            else if (cubie->gridY == -1) layerBottom.push_back(cubie);
            else                         layerMiddleY.push_back(cubie);

            if (cubie->gridX == -1)      layerLeft.push_back(cubie);
            else if (cubie->gridX == 1)  layerRight.push_back(cubie);
            else                         layerMiddleX.push_back(cubie);

            if (cubie->gridZ == -1)      layerFront.push_back(cubie);
            else if (cubie->gridZ == 1)  layerBack.push_back(cubie);
            else                         layerMiddleZ.push_back(cubie);
        }
    }

    // Helper to generate the current matrix transformation for animating blocks
    myglm::mat4 getActiveAnimationMatrix() {
        if (!isAnimating) return myglm::mat4(1.0f);
        return myglm::rotate(myglm::mat4(1.0f), myglm::radians(animationProgressAngle), currentAnimationAxis);
    }

    ~RubikCube() {
        for (Cubie* piece : cubies) delete piece;
    }
};

class Camera {
public:
    // Core Vectors
    myglm::vec3 position;
    myglm::vec3 target;
    myglm::vec3 up;

    // Orbital Spherical Coordinates
    float radius;
    float angleX; // Elevation / Pitch (Rotation around global X/Right axis)
    float angleY; // Azimuth / Yaw (Rotation around global Y/Up axis)

    // Constructor
    Camera(myglm::vec3 targetPos = myglm::vec3(0.0f, 0.0f, 0.0f), float initialRadius = 3.0f)
        : target(targetPos), radius(initialRadius), angleX(0.0f), angleY(0.0f), up(0.0f, 1.0f, 0.0f) {
        updateCameraVectors();
    }

    // 1. Calculate camera position based on target, radius, and angles
    void updateCameraVectors() {
        // Spherical to Cartesian coordinates conversion
        position.x = target.x + radius * std::cos(angleX) * std::sin(angleY);
        position.y = target.y + radius * std::sin(angleX);
        position.z = target.z + radius * std::cos(angleX) * std::cos(angleY);

        // Handle the gimbal flip when looking directly upside down
        if (std::cos(angleX) < 0.0f) {
            up = myglm::vec3(0.0f, -1.0f, 0.0f);
        } else {
            up = myglm::vec3(0.0f, 1.0f, 0.0f);
        }
    }

    // 2. Return the calculated view matrix
    myglm::mat4 getViewMatrix() {
        return myglm::lookAt(position, target, up);
    }

    // 3. Orbiting Controls (Changes view angle around the target)
    void orbitX(float amount) {
        angleX += amount;
        
        // Clamp elevation to prevent passing directly over the poles
        if (angleX > 2.0f * myglm::PI) angleX -= 2.0f * myglm::PI;
        if (angleX < -2.0f * myglm::PI) angleX += 2.0f * myglm::PI;

        updateCameraVectors();
    }

    void orbitY(float amount) {
        angleY += amount;
        updateCameraVectors();
    }

    // 4. Zoom Control (Changes the distance to the target)
    void adjustRadius(float amount) {
        radius += amount;
        if (radius < 0.1f) radius = 0.1f; // Prevent reverse/clipping camera
        updateCameraVectors();
    }

    // 5. Linear Movement / Panning (Moves both camera and target together)
    void moveTarget(myglm::vec3 translationOffset) {
        target += translationOffset;
        updateCameraVectors(); // Re-center position relative to new target
    }
};

class Scene {
public:
    std::vector<Shape*> shapePointers;
    Camera camera;
    myglm::mat4 projection;
    
    int selectedShape = 0;
    unsigned int lastShaderProgram = 0;
    
    // Tracks if scene-wide uniforms (view/projection) need a refresh
    bool isSceneDirty = true;

    // Animation and Timing Variables
    bool animationIsRunning = false;
    float lastTime = 0.0f;
    float deltaTime = 0.0f;

    // Caching Uniform Locations to avoid glGetUniformLocation every frame
    unsigned int viewLoc = 0;
    unsigned int projLoc = 0;

    Scene(float fovy = myglm::radians(45.0f), float aspect = (float)WIDTH / (float)HEIGHT, float zNear = 0.1f, float zFar = 100.0f) {
        projection = myglm::perspective(fovy, aspect, zNear, zFar);
        lastTime = (float)glfwGetTime();
    }

    void updateCameraOrbit() {
        // Extern boolean flags declared globally
        extern bool g_LeftPressed, g_RightPressed, g_UpPressed, g_DownPressed;

        const float cameraOrbitSpeed = 2.0f; // Radians per second
        float speedFactor = cameraOrbitSpeed * deltaTime;

        // The math only executes if a flag is active!
        if (g_LeftPressed)  { camera.orbitY(-speedFactor); isSceneDirty = true; }
        if (g_RightPressed) { camera.orbitY(speedFactor);  isSceneDirty = true; }
        if (g_UpPressed)    { camera.orbitX(speedFactor);  isSceneDirty = true; }
        if (g_DownPressed)  { camera.orbitX(-speedFactor); isSceneDirty = true; }
    }

    // 1. Core Runtime Frame Cycle
    void run(unsigned int shaderProgram) {
        updateTime();
        updateCameraOrbit();
        
        // 1. Advance the animation state by updating elapsed delta time
        if (g_RubikInstance) {
            g_RubikInstance->updateAnimation(deltaTime);
        }

        updateGlobalMatrices(shaderProgram);

        // 2. Generate current dynamic matrix transformation configuration
        myglm::mat4 animMatrix = g_RubikInstance ? g_RubikInstance->getActiveAnimationMatrix() : myglm::mat4(1.0f);

        // 3. Cast to Cubie pointer and render using our updated layout parameters
        for (size_t i = 0; i < shapePointers.size(); ++i) {
            Cubie* cubie = static_cast<Cubie*>(shapePointers[i]);
            cubie->draw(shaderProgram, animMatrix);
        }
    }

    // 2. Track Elapsed Time
    void updateTime() {
        float currentTime = (float)glfwGetTime();
        deltaTime = currentTime - lastTime;
        lastTime = currentTime;
    }

    // 3. Toggle Animation Engine State
    void toggleAnimation() {
        animationIsRunning = !animationIsRunning;
        // Reset lastTime on unpause to avoid a massive deltaTime spike jump
        if (animationIsRunning) {
            lastTime = (float)glfwGetTime();
        }
    }

    // 4. Time-Delta Dependent Animation Logic
    void handleAnimation() {
        if (!animationIsRunning || shapePointers.empty()) {
            return;
        }
        
        // Example: Continuously spin the very first shape safely based on delta time
        // shapePointers[0]->rotate(myglm::vec3(0.0f, 1.0f * deltaTime, 0.0f));
    }

    // 5. Update Uniform Matrices ONLY on Switch or Change
    void updateGlobalMatrices(unsigned int shaderProgram) {
        // If the shader changed, we MUST grab fresh locations and force an update
        if (lastShaderProgram != shaderProgram) {
            glUseProgram(shaderProgram);
            lastShaderProgram = shaderProgram;
            
            viewLoc = glGetUniformLocation(shaderProgram, "view");
            projLoc = glGetUniformLocation(shaderProgram, "projection");
            
            isSceneDirty = true; 
        }

        // Only stream data to GPU if the view/projection layout changed
        if (isSceneDirty) {
            myglm::mat4 view = camera.getViewMatrix();
            
            glUniformMatrix4fv(viewLoc, 1, GL_FALSE, myglm::value_ptr(view));
            glUniformMatrix4fv(projLoc, 1, GL_FALSE, myglm::value_ptr(projection));
            
            isSceneDirty = false; // Reset scene state flag
        }
    }

    // Wrapper methods to interact with camera safely and alert the scene to changes
    void orbitCameraX(float amount) {
        camera.orbitX(amount);
        isSceneDirty = true;
    }

    void orbitCameraY(float amount) {
        camera.orbitY(amount);
        isSceneDirty = true;
    }

    void zoomCamera(float amount) {
        camera.adjustRadius(amount);
        isSceneDirty = true;
    }

    void panCameraTarget(myglm::vec3 offset) {
        camera.moveTarget(offset);
        isSceneDirty = true;
    }
};


void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
    glViewport(0, 0, width, height);
    if (g_SceneInstance && height > 0) {
        float aspect = static_cast<float>(width) / static_cast<float>(height);
        g_SceneInstance->projection = myglm::perspective(myglm::radians(45.0f), aspect, 0.1f, 100.0f);
        g_SceneInstance->isSceneDirty = true;
    }
}

void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    if (!g_SceneInstance || !g_RubikInstance) return;

    // 1. Track CONTINUOUS actions (Press and Release)
    if (action == GLFW_PRESS || action == GLFW_RELEASE) {
        bool isPressed = (action == GLFW_PRESS);
        
        switch (key) {
            case GLFW_KEY_LEFT:  g_LeftPressed  = isPressed; return;
            case GLFW_KEY_RIGHT: g_RightPressed = isPressed; return;
            case GLFW_KEY_UP:    g_UpPressed    = isPressed; return;
            case GLFW_KEY_DOWN:  g_DownPressed  = isPressed; return;
            default: break;
        }
    }

    // 2. Track DISCRETE actions (Only on initial press)
    if (action != GLFW_PRESS) return;

    bool shiftPressed = (mods & GLFW_MOD_SHIFT) != 0;
    float angle = shiftPressed ? -90.0f : 90.0f;

    switch (key) {
        // --- System Controls ---
        case GLFW_KEY_ESCAPE:
            glfwSetWindowShouldClose(window, GLFW_TRUE);
            break;

        // --- Camera Zoom Controls ---
        case GLFW_KEY_KP_ADD:       
        case GLFW_KEY_EQUAL:        
            g_SceneInstance->zoomCamera(-0.2f); 
            break;
        case GLFW_KEY_MINUS:        
        case GLFW_KEY_KP_SUBTRACT:  
            g_SceneInstance->zoomCamera(0.2f);  
            break;

        // --- Rubik's Cube Layer Rotations ---
        case GLFW_KEY_Q:
            g_RubikInstance->queueLayerRotation(g_RubikInstance->layerLeft, angle, myglm::vec3(1.0f, 0.0f, 0.0f));
            break;
        case GLFW_KEY_W:
            g_RubikInstance->queueLayerRotation(g_RubikInstance->layerMiddleX, angle, myglm::vec3(1.0f, 0.0f, 0.0f));
            break;
        case GLFW_KEY_E:
            g_RubikInstance->queueLayerRotation(g_RubikInstance->layerRight, angle, myglm::vec3(1.0f, 0.0f, 0.0f));
            break;

        case GLFW_KEY_A:
            g_RubikInstance->queueLayerRotation(g_RubikInstance->layerBottom, angle, myglm::vec3(0.0f, 1.0f, 0.0f));
            break;
        case GLFW_KEY_S:
            g_RubikInstance->queueLayerRotation(g_RubikInstance->layerMiddleY, angle, myglm::vec3(0.0f, 1.0f, 0.0f));
            break;
        case GLFW_KEY_D:
            g_RubikInstance->queueLayerRotation(g_RubikInstance->layerTop, angle, myglm::vec3(0.0f, 1.0f, 0.0f));
            break;

        case GLFW_KEY_Z:
            g_RubikInstance->queueLayerRotation(g_RubikInstance->layerFront, angle, myglm::vec3(0.0f, 0.0f, 1.0f));
            break;
        case GLFW_KEY_X:
            g_RubikInstance->queueLayerRotation(g_RubikInstance->layerMiddleZ, angle, myglm::vec3(0.0f, 0.0f, 1.0f));
            break;
        case GLFW_KEY_C:
            g_RubikInstance->queueLayerRotation(g_RubikInstance->layerBack, angle, myglm::vec3(0.0f, 0.0f, 1.0f));
            break;

        default:
            break;
    }
}

int main() {
    // 1. Initialize GLFW Context
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        return -1;
    }

    // Set minimal OpenGL Hints to ensure modern Context Profiling
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(WIDTH, HEIGHT, "Rubik's Cube Visualization", NULL, NULL);
    if (!window) {
        std::cerr << "Failed to create GLFW Window" << std::endl;
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);
    
    if (!gladLoadGL(glfwGetProcAddress)) {
        std::cout << "Failed to initialize GLAD" << std::endl;
        return -1;
    }

    glfwSwapInterval(1); // Enable V-Sync

	{

		// 2. Configure Global OpenGL State Machine Flags
		glEnable(GL_DEPTH_TEST); // Ensures background faces don't draw over front faces
		glPointSize(5.0f);       // Make vertex markers visible
		glClearColor(0.2f, 0.25f, 0.3f, 1.0f); // Slate Blue Canvas Background

		// 3. Initialize Scene Manager Component
		Scene myScene;
		g_SceneInstance = &myScene; // Link global pointer for window tracking
		glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);

		// 4. Instantiate the Rubik's Cube Structural Array
		RubikCube rubik(myglm::vec3(0.0f, 0.0f, 0.0f), 1.0f);
        g_RubikInstance = &rubik;

        glfwSetKeyCallback(window, key_callback);

		// Transfer the cubie pointers into the Scene shape array so they can be drawn automatically
		for (Cubie* cubie : rubik.cubies) {
			myScene.shapePointers.push_back(cubie);
		}

		// 5. Compile and Link Shaders
		// Make sure "shader.vert" and "shader.frag" are located in your executable path directory!
		unsigned int shaderProgram = loadShaders("shader.vert", "shader.frag");
		if (shaderProgram == 0) {
			std::cerr << "Shader fallback: Could not initiate application pipeline." << std::endl;
			glfwTerminate();
			return -1;
		}

		// 6. Primary Execution Application Loop
		while (!glfwWindowShouldClose(window)) {
			// Clear frame color and reset depth buffer components
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

			// Update matrices and draw all 27 elements
			myScene.run(shaderProgram);

			glfwSwapBuffers(window);
			glfwPollEvents();
		}

		// 7. Resource Cleanup Destruction State
		glDeleteProgram(shaderProgram);

	}
	
    glfwDestroyWindow(window);
    glfwTerminate();
    
    return 0;
}