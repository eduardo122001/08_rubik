#define GLAD_GL_IMPLEMENTATION
#include <glad/gl.h>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include "myglm.h"

#include "app_runner.h"

#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <cstdlib>
#include <queue>

#define WIDTH 1000
#define HEIGHT 800

class Scene;
class RubikCube;
struct CubeMove;
//class RubikSolver;

Scene* g_SceneInstance = nullptr;
RubikCube* g_RubikInstance = nullptr;
std::vector<std::string> g_MoveHistory;

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
    friend class RubikSolver;
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

    // Returns the color currently facing the requested absolute world direction.
    myglm::vec4 getColorFacingWorldDirection(const myglm::vec3& targetWorldDir) const {
        // 'static const' prevents the CPU from allocating and destroying this array 
        // every single time the function is called, significantly boosting performance.
        static const myglm::vec3 originalFaceDirs[6] = {
            myglm::vec3( 0.0f,  0.0f, -1.0f), // 0: Back (-Z)  [Corrected from Front]
            myglm::vec3( 0.0f,  0.0f,  1.0f), // 1: Front (+Z) [Corrected from Back]
            myglm::vec3(-1.0f,  0.0f,  0.0f), // 2: Left (-X)
            myglm::vec3( 1.0f,  0.0f,  0.0f), // 3: Right (+X)
            myglm::vec3( 0.0f,  1.0f,  0.0f), // 4: Up (+Y)
            myglm::vec3( 0.0f, -1.0f,  0.0f)  // 5: Down (-Y)
        };

        // Initialized to a very low value to safely handle non-normalized, scaled dot products
        float bestDot = -1000.0f; 
        int matchingFaceIdx = 0;

        for (int i = 0; i < 6; ++i) {
            // 1. Pack direction (w=0 ignores translations) and rotate by the orientation matrix
            myglm::vec4 transformedVec4 = homeMatrix * myglm::vec4(originalFaceDirs[i], 0.0f);
            
            // 2. Extract into vec3 to perform the dot product
            myglm::vec3 currentWorldDir(transformedVec4.x, transformedVec4.y, transformedVec4.z);
            
            // 3. Measure alignment (acts as a scaled cosine similarity)
            float dotVal = myglm::dot(currentWorldDir, targetWorldDir);
            
            if (dotVal > bestDot) {
                bestDot = dotVal;
                matchingFaceIdx = i;
            }
        }

        return faceColors[matchingFaceIdx];
    }
};

struct CubeMove {
    std::string notation; // e.g., "R", "U'", "M2"
    std::vector<Cubie*>* layerBatch;
    float angle;
    myglm::vec3 axis;
};

class RubikCube {
public:
    std::vector<Cubie*> cubies;

    // All 9 Layer Rotation Batches fully restored
    std::vector<Cubie*> layerTop;    std::vector<Cubie*> layerMiddleY; std::vector<Cubie*> layerBottom;
    std::vector<Cubie*> layerLeft;   std::vector<Cubie*> layerMiddleX; std::vector<Cubie*> layerRight;
    std::vector<Cubie*> layerFront;  std::vector<Cubie*> layerMiddleZ; std::vector<Cubie*> layerBack;

    float sizeSingleCubie;

    // Animation Properties
    bool isAnimating = false;
    std::vector<Cubie*>* currentAnimatedBatch = nullptr;
    myglm::vec3 currentAnimationAxis = myglm::vec3(0.0f);
    
    float animationProgressAngle = 0.0f;
    float animationTargetAngle = 0.0f;
    float rotationSpeed = 1800.0f; 

    std::queue<CubeMove> moveQueue;

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

        for (int i = 0; i < 3; i++) {       
            for (int j = 0; j < 3; j++) {   
                for (int k = 0; k < 3; k++) { 
                    myglm::vec3 targetCubiePos(
                        centerPosition.x + (k - 1) * sizeSingleCubie,
                        centerPosition.y + (j - 1) * sizeSingleCubie,
                        centerPosition.z + (i - 1) * sizeSingleCubie
                    );

                    std::vector<myglm::vec4> assignedColors(6, structuralInternalBlack);
                    if (i == 0) assignedColors[0] = palette[2]; // Back (-Z) is Blue
                    if (i == 2) assignedColors[1] = palette[4]; // Front (+Z) is Green
                    if (k == 0) assignedColors[2] = palette[1]; // Left (-X) is Orange
                    if (k == 2) assignedColors[3] = palette[3]; // Right (+X) is Red
                    if (j == 2) assignedColors[4] = palette[0]; // Up (+Y) is White
                    if (j == 0) assignedColors[5] = palette[5]; // Down (-Y) is Yellow

                    Cubie* newCubie = new Cubie(targetCubiePos, cubieScale, initialRotation, assignedColors);
                    
                    newCubie->gridX = k - 1; 
                    newCubie->gridY = j - 1; 
                    newCubie->gridZ = i - 1; 

                    cubies.push_back(newCubie);
                }
            }
        }
        updateTrackingBatches();
    }

    void parseAndQueueMove(const std::string& moveStr) {
        if (moveStr.empty()) return;

        CubeMove move;
        move.notation = moveStr;
        
        char face = moveStr[0];
        bool isPrime = (moveStr.size() > 1 && moveStr[1] == '\'');
        bool isDouble = (moveStr.size() > 1 && moveStr[1] == '2');
        
        float baseAngle = 90.0f;
        if (isPrime) baseAngle = -90.0f;
        if (isDouble) baseAngle = 180.0f; // Handled as an absolute value for the animator sweep

        switch (face) {
            // U passes negative baseAngle, D passes positive baseAngle
            case 'U': move.layerBatch = &layerTop;     move.axis = myglm::vec3(0.0f, 1.0f, 0.0f);  move.angle = -baseAngle; break;
            case 'D': move.layerBatch = &layerBottom;  move.axis = myglm::vec3(0.0f, 1.0f, 0.0f);  move.angle = baseAngle;  break;
            
            // R passes negative baseAngle, L passes positive baseAngle
            case 'R': move.layerBatch = &layerRight;   move.axis = myglm::vec3(1.0f, 0.0f, 0.0f);  move.angle = -baseAngle; break;
            case 'L': move.layerBatch = &layerLeft;    move.axis = myglm::vec3(1.0f, 0.0f, 0.0f);  move.angle = baseAngle;  break;
            
            // F passes negative baseAngle, B passes positive baseAngle
            case 'F': move.layerBatch = &layerFront;   move.axis = myglm::vec3(0.0f, 0.0f, 1.0f);  move.angle = -baseAngle; break;
            case 'B': move.layerBatch = &layerBack;    move.axis = myglm::vec3(0.0f, 0.0f, 1.0f);  move.angle = baseAngle;  break;
            
            // Middle section assignments matching parallel faces
            case 'M': move.layerBatch = &layerMiddleX; move.axis = myglm::vec3(1.0f, 0.0f, 0.0f);  move.angle = baseAngle;  break; // Follows L
            case 'E': move.layerBatch = &layerMiddleY; move.axis = myglm::vec3(0.0f, 1.0f, 0.0f);  move.angle = baseAngle;  break; // Follows D
            case 'S': move.layerBatch = &layerMiddleZ; move.axis = myglm::vec3(0.0f, 0.0f, 1.0f);  move.angle = -baseAngle; break; // Follows F
            default: return;
        }

        moveQueue.push(move);
    }

    void queueLayerRotation(std::vector<Cubie*>& targetBatch, float angleDegrees, const myglm::vec3& axis) {
        if (isAnimating) return; 
        isAnimating = true;
        currentAnimatedBatch = &targetBatch;
        currentAnimationAxis = axis;
        animationProgressAngle = 0.0f;
        animationTargetAngle = angleDegrees;

        for (Cubie* cubie : *currentAnimatedBatch) {
            cubie->isAnimating = true;
        }
    }

    void updateAnimation(float deltaTime) {
        // If the system is idle but moves are waiting in the pipeline, pop the next turn
        if (!isAnimating && !moveQueue.empty()) {
            CubeMove nextMove = moveQueue.front();
            moveQueue.pop();
            queueLayerRotation(*nextMove.layerBatch, nextMove.angle, nextMove.axis);
        }

        if (!isAnimating) return;

        // Progress the smooth interpolation sweep over elapsed time
        float direction = (animationTargetAngle > 0.0f) ? 1.0f : -1.0f;
        animationProgressAngle += direction * rotationSpeed * deltaTime;

        // Check if the target angle has been met
        bool animationFinished = false;
        if (direction > 0.0f && animationProgressAngle >= animationTargetAngle) {
            animationProgressAngle = animationTargetAngle;
            animationFinished = true;
        } else if (direction < 0.0f && animationProgressAngle <= animationTargetAngle) {
            animationProgressAngle = animationTargetAngle;
            animationFinished = true;
        }

        // When the animation completes, bake the changes into the structural layouts
        if (animationFinished) {
            float finalRad = myglm::radians(animationTargetAngle);
            myglm::mat4 finalRotMatrix = myglm::rotate(myglm::mat4(1.0f), finalRad, currentAnimationAxis);

            for (Cubie* cubie : *currentAnimatedBatch) {
                cubie->isAnimating = false;
                
                // 1. Commit the exact 90° or 180° rotation to the visual baseline matrix
                cubie->homeMatrix = finalRotMatrix * cubie->homeMatrix;

                // 2. Transform the logical tracking coordinates using the EXACT SAME matrix
                myglm::vec4 currentLogicalPos(
                    static_cast<float>(cubie->gridX), 
                    static_cast<float>(cubie->gridY), 
                    static_cast<float>(cubie->gridZ), 
                    1.0f
                );
                
                myglm::vec4 updatedLogicalPos = finalRotMatrix * currentLogicalPos;

                // 3. Round to the nearest integer to eliminate floating-point precision drift
                cubie->gridX = static_cast<int>(std::round(updatedLogicalPos.x));
                cubie->gridY = static_cast<int>(std::round(updatedLogicalPos.y));
                cubie->gridZ = static_cast<int>(std::round(updatedLogicalPos.z));
            }

            // Reset animation tracking flags and clear/refresh structural layers
            isAnimating = false;
            currentAnimatedBatch = nullptr;
            animationProgressAngle = 0.0f;
            updateTrackingBatches();
        }
    }

    void updateTrackingBatches() {
        layerTop.clear();    layerMiddleY.clear(); layerBottom.clear();
        layerLeft.clear();   layerMiddleX.clear(); layerRight.clear();
        layerFront.clear();  layerMiddleZ.clear(); layerBack.clear();

        for (Cubie* cubie : cubies) {
            // Y-Axis
            if (cubie->gridY == 1)       layerTop.push_back(cubie);
            else if (cubie->gridY == -1) layerBottom.push_back(cubie);
            else                         layerMiddleY.push_back(cubie);

            // X-Axis
            if (cubie->gridX == -1)      layerLeft.push_back(cubie);
            else if (cubie->gridX == 1)  layerRight.push_back(cubie);
            else                         layerMiddleX.push_back(cubie);

            // Z-Axis (CORRECTED: Z = 1 is Front Blue, Z = -1 is Back Green)
            if (cubie->gridZ == 1)       layerFront.push_back(cubie);
            else if (cubie->gridZ == -1) layerBack.push_back(cubie);
            else                         layerMiddleZ.push_back(cubie);
        }
    }

    void scramble(int movesCount = 20) {
        std::string movePool[] = {
            "U", "U'", "U2",
            "D", "D'", "D2",
            "R", "R'", "R2",
            "L", "L'", "L2",
            "F", "F'", "F2",
            "B", "B'", "B2",
            "M", "M'", "M2",
            "E", "E'", "E2",
            "S", "S'", "S2"
        };

        // Ensure the global history array knows about this sequence
        extern std::vector<std::string> g_MoveHistory; 

        for (int i = 0; i < movesCount; i++) {
            int randIndex = std::rand() % 27;
            std::string randomMove = movePool[randIndex];
            
            // 1. Send to your visualizer queue
            parseAndQueueMove(randomMove);
            
            // 2. Track it in your historical reversal stack
            g_MoveHistory.push_back(randomMove);
        }
    }

    myglm::mat4 getActiveAnimationMatrix() {
        if (!isAnimating) return myglm::mat4(1.0f);
        return myglm::rotate(myglm::mat4(1.0f), myglm::radians(animationProgressAngle), currentAnimationAxis);
    }
/*
    char convertVectorColorToChar(const myglm::vec4& color) {
        // Reference your palette values exactly from your constructor setup:
        // palette[0] = White, palette[1] = Orange, palette[2] = Blue, 
        // palette[3] = Red,   palette[4] = Green,  palette[5] = Yellow
        
        if (std::abs(color.x - 1.0f) < 0.01f && std::abs(color.y - 1.0f) < 0.01f && std::abs(color.z - 1.0f) < 0.01f) return 'U'; // White (Up)
        if (std::abs(color.x - 1.0f) < 0.01f && std::abs(color.y - 0.0f) < 0.01f && std::abs(color.z - 0.0f) < 0.01f) return 'R'; // Red (Right)
        if (std::abs(color.x - 0.0f) < 0.01f && std::abs(color.y - 0.0f) < 0.01f && std::abs(color.z - 1.0f) < 0.01f) return 'F'; // Blue (Front)
        if (std::abs(color.x - 1.0f) < 0.01f && std::abs(color.y - 1.0f) < 0.01f && std::abs(color.z - 0.0f) < 0.01f) return 'D'; // Yellow (Down)
        if (std::abs(color.x - 1.0f) < 0.01f && std::abs(color.y - 0.5f) < 0.01f && std::abs(color.z - 0.0f) < 0.01f) return 'L'; // Orange (Left)
        if (std::abs(color.x - 0.0f) < 0.01f && std::abs(color.y - 1.0f) < 0.01f && std::abs(color.z - 0.0f) < 0.01f) return 'B'; // Green (Back)
        
        return 'X'; // Internal black or error fallback
    }
*/
    Cubie* getCubieAt(int x, int y, int z) {
        for (Cubie* cubie : cubies) {
            if (cubie->gridX == x && cubie->gridY == y && cubie->gridZ == z) {
                return cubie;
            }
        }
        return nullptr;
    }
/*
    std::string generateFaceletString() {
        std::string faceletString = "";
        faceletString.reserve(54); // Memory optimization allocation

        // Define scanning sequences to match standard U-R-F-D-L-B facelet requirements
        
        // 1. UP FACE (White)
        for (int z = -1; z <= 1; ++z) {
            for (int x = -1; x <= 1; ++x) {
                Cubie* c = getCubieAt(x, 1, z);
                faceletString += convertVectorColorToChar(c->getColorFacingWorldDirection(myglm::vec3(0.0f, 1.0f, 0.0f)));
            }
        }

        // 2. RIGHT FACE (Red)
        for (int y = 1; y >= -1; --y) {
            for (int z = -1; z <= 1; ++z) {
                Cubie* c = getCubieAt(1, y, z);
                faceletString += convertVectorColorToChar(c->getColorFacingWorldDirection(myglm::vec3(1.0f, 0.0f, 0.0f)));
            }
        }

        // 3. FRONT FACE (Blue)
        for (int y = 1; y >= -1; --y) {
            for (int x = -1; x <= 1; ++x) {
                Cubie* c = getCubieAt(x, y, 1);
                faceletString += convertVectorColorToChar(c->getColorFacingWorldDirection(myglm::vec3(0.0f, 0.0f, 1.0f)));
            }
        }

        // 4. DOWN FACE (Yellow)
        for (int z = 1; z >= -1; --z) {
            for (int x = -1; x <= 1; ++x) {
                Cubie* c = getCubieAt(x, -1, z);
                faceletString += convertVectorColorToChar(c->getColorFacingWorldDirection(myglm::vec3(0.0f, -1.0f, 0.0f)));
            }
        }

        // 5. LEFT FACE (Orange)
        for (int y = 1; y >= -1; --y) {
            for (int z = 1; z >= -1; --z) {
                Cubie* c = getCubieAt(-1, y, z);
                faceletString += convertVectorColorToChar(c->getColorFacingWorldDirection(myglm::vec3(-1.0f, 0.0f, 0.0f)));
            }
        }

        // 6. BACK FACE (Green)
        for (int y = 1; y >= -1; --y) {
            for (int x = 1; x >= -1; --x) {
                Cubie* c = getCubieAt(x, y, -1);
                faceletString += convertVectorColorToChar(c->getColorFacingWorldDirection(myglm::vec3(0.0f, 0.0f, -1.0f)));
            }
        }

        return faceletString;
    }
*/
    bool getSolverState(uint8_t* solverState) {

        Cubie* cU = getCubieAt(0, 1, 0);
        Cubie* cL = getCubieAt(-1, 0, 0);
        Cubie* cF = getCubieAt(0, 0, 1);
        Cubie* cR = getCubieAt(1, 0, 0);
        Cubie* cB = getCubieAt(0, 0, -1);
        Cubie* cD = getCubieAt(0, -1, 0);

        myglm::vec4 uColor = cU->getColorFacingWorldDirection(myglm::vec3(0, 1, 0));
        myglm::vec4 lColor = cL->getColorFacingWorldDirection(myglm::vec3(-1, 0, 0));
        myglm::vec4 fColor = cF->getColorFacingWorldDirection(myglm::vec3(0, 0, 1));
        myglm::vec4 rColor = cR->getColorFacingWorldDirection(myglm::vec3(1, 0, 0));
        myglm::vec4 bColor = cB->getColorFacingWorldDirection(myglm::vec3(0, 0, -1));
        myglm::vec4 dColor = cD->getColorFacingWorldDirection(myglm::vec3(0, -1, 0));

        // Helper 1: Map any color to the correct ID based on center parity
        auto colorToId = [&](const myglm::vec4& c) -> uint8_t {
            auto dist = [](const myglm::vec4& a, const myglm::vec4& b) {
                return std::abs(a.x-b.x) + std::abs(a.y-b.y) + std::abs(a.z-b.z);
            };
            if (dist(c, uColor) < 0.1f) return 1;
            if (dist(c, lColor) < 0.1f) return 2;
            if (dist(c, fColor) < 0.1f) return 3;
            if (dist(c, rColor) < 0.1f) return 4;
            if (dist(c, bColor) < 0.1f) return 5;
            if (dist(c, dColor) < 0.1f) return 6;
            return 1; // Fallback
        };

        // Helper 2: Safely extract a cubie from the grid without crashing if it drifts
        auto safeGetColor = [&](int x, int y, int z, myglm::vec3 dir) -> uint8_t {
            Cubie* c = getCubieAt(x, y, z);
            if (!c) {
                std::cout << "[FATAL ERROR] Missing cubie at Grid(" << x << "," << y << "," << z << ").\n";
                return 1; // Fallback
            }
            return colorToId(c->getColorFacingWorldDirection(dir));
        };

        // This array translates standard Row-Major loops into the solver's bizarre Spiral layout
        const int spiralMap[9] = {0, 1, 2, 7, 8, 3, 6, 5, 4};
        
        int blockStart = 0;
        int rmIdx = 0;
        
        // 1. U Face (0..8) - Top is Back (-Z), Bottom is Front (+Z). Left is Left (-X).
        blockStart = 0; rmIdx = 0;
        for (int z = -1; z <= 1; ++z) 
            for (int x = -1; x <= 1; ++x) 
                solverState[blockStart + spiralMap[rmIdx++]] = safeGetColor(x, 1, z, myglm::vec3(0, 1, 0));
        
        // 2. L Face (9..17) - Top is Up (+Y). Left is Back (-Z).
        blockStart = 9; rmIdx = 0;
        for (int y = 1; y >= -1; --y) 
            for (int z = -1; z <= 1; ++z) 
                solverState[blockStart + spiralMap[rmIdx++]] = safeGetColor(-1, y, z, myglm::vec3(-1, 0, 0));
        
        // 3. F Face (18..26) - Top is Up (+Y). Left is Left (-X).
        blockStart = 18; rmIdx = 0;
        for (int y = 1; y >= -1; --y) 
            for (int x = -1; x <= 1; ++x) 
                solverState[blockStart + spiralMap[rmIdx++]] = safeGetColor(x, y, 1, myglm::vec3(0, 0, 1));
        
        // 4. R Face (27..35) - Top is Up (+Y). Left is Front (+Z).
        blockStart = 27; rmIdx = 0;
        for (int y = 1; y >= -1; --y) 
            for (int z = 1; z >= -1; --z) 
                solverState[blockStart + spiralMap[rmIdx++]] = safeGetColor(1, y, z, myglm::vec3(1, 0, 0));
        
        // 5. B Face (36..44) - Top is Up (+Y). Left is Right (+X).
        blockStart = 36; rmIdx = 0;
        for (int y = 1; y >= -1; --y) 
            for (int x = 1; x >= -1; --x) 
                solverState[blockStart + spiralMap[rmIdx++]] = safeGetColor(x, y, -1, myglm::vec3(0, 0, -1));
        
        // 6. D Face (45..53) - Top is Front (+Z), Bottom is Back (-Z). Left is Left (-X).
        blockStart = 45; rmIdx = 0;
        for (int z = 1; z >= -1; --z) 
            for (int x = -1; x <= 1; ++x) 
                solverState[blockStart + spiralMap[rmIdx++]] = safeGetColor(x, -1, z, myglm::vec3(0, -1, 0));

        // DEBUG PRINTER
        std::cout << "[DEBUG] Generated Spiral Array: \n";
        for(int i = 0; i < 6; i++) {
            for(int j = 0; j < 9; j++) std::cout << (int)solverState[i*9 + j] << ", ";
            std::cout << "\n";
        }

        return true; 
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

void queueExternalSolution(const std::string& solutionString) {
    if (solutionString.empty() || solutionString.find("Error") != std::string::npos) {
        std::cout << "[SOLVER BRIDGE] Invalid or impossible layout string detected." << std::endl;
        return;
    }

    std::stringstream ss(solutionString);
    std::string currentMove;

    std::cout << "[SOLVER BRIDGE] Pushing translated moves to engine: " << std::endl;

    // Tokenize space-separated solution moves smoothly
    while (ss >> currentMove) {
        // Translate the solver's 'i' (inverse) into the visualizer's standard prime symbol '\''
        if (currentMove.size() > 1 && currentMove[1] == 'i') {
            currentMove[1] = '\'';
        }
        // Feed the standardized move directly into your robust animation queue
        g_RubikInstance->parseAndQueueMove(currentMove);
        // If you want the solver's moves to be reversible by your 'O' key, uncomment the line below:
        // g_MoveHistory.push_back(currentMove); 
    }
}

std::string getInverseMove(const std::string& move) {
    if (move.empty()) return "";
    
    // If it's a prime move (e.g., "R'"), drop the prime to make it normal ("R")
    if (move.back() == '\'') {
        return move.substr(0, move.size() - 1);
    } 
    // If it's a double move (e.g., "R2"), the inverse is just "R2"
    else if (move.back() == '2') {
        return move; 
    } 
    // If it's a standard move (e.g., "R"), add a prime to invert it ("R'")
    else {
        return move + "'";
    }
}

void triggerZZOptimalSolver() {
    if (!g_RubikInstance || g_RubikInstance->isAnimating) {
        std::cout << "[SYSTEM] Solver rejected: Cube is currently animating." << std::endl;
        return;
    }

    uint8_t solverState[54];
    
    // If the 3D grid was corrupted, abort before touching the engine
    if (!g_RubikInstance->getSolverState(solverState)) {
        return; 
    }

    DirectSolveConfig config = make_direct_solve_config(
        SolverMode::Normal, 
        PatternPreset::None,
        false, false, false
    );

    SolveStats stats = {};
    std::cout << "[SYSTEM] Running zz-optimal-core solver..." << std::endl;
    
    bool ok = app_run_solve_from_state(solverState, config, &stats);

    if (ok) {
        if (stats.total_moves == 0) {
            std::cout << "[SUCCESS] Cube is already solved!" << std::endl;
        } else {
            std::cout << "[SUCCESS] Solution found in " << stats.total_moves << " moves." << std::endl;
            queueExternalSolution(stats.solution);
        }
    } else {
        std::cout << "[ERROR] Engine cleanly rejected the array. The physical state is impossible." << std::endl;
    }
}
/*
void triggerZZOptimalSolver2() {
    std::cout << "[SYSTEM] Running Hardcoded Diagnostic Test..." << std::endl;

    // A mathematically perfect scramble provided by the zz-optimal-core tutorial
    const uint8_t test_scramble[54] = {
        1, 1, 3, 1, 1, 3, 1, 1, 3, 
        2, 2, 2, 2, 2, 2, 2, 2, 2, 
        3, 3, 6, 3, 3, 6, 3, 3, 6, 
        4, 4, 4, 4, 4, 4, 4, 4, 4, 
        1, 5, 5, 1, 5, 5, 1, 5, 5, 
        6, 6, 5, 6, 6, 5, 6, 6, 5
    };

    DirectSolveConfig config = make_direct_solve_config(
        SolverMode::Normal, 
        PatternPreset::None,
        false, false, false
    );

    SolveStats stats = {};
    
    // Execute the solver on the HARDCODED state, ignoring the visualizer
    bool ok = app_run_solve_from_state(test_scramble, config, &stats);

    if (ok) {
        std::cout << "[SUCCESS] Engine is working perfectly! Solution: " << stats.solution << std::endl;
        std::cout << "[CONCLUSION] The silent crash is 100% caused by the visualizer array generation mapping." << std::endl;
    } else {
        std::cout << "[ERROR] Engine failed to solve a known valid state." << std::endl;
    }
}

void triggerZZOptimalSolver3() {
    std::cout << "[SYSTEM] Running Hardcoded Diagnostic Test..." << std::endl;

    // A mathematically perfect scramble provided by the zz-optimal-core tutorial
    const uint8_t test_scramble[54] = {
        6, 1, 1, 1, 6, 6, 4, 6, 1,
        4, 4, 1, 2, 1, 5, 1, 3, 2,
        3, 3, 3, 3, 2, 2, 3, 6, 3,
        2, 5, 5, 4, 4, 4, 6, 2, 4,
        2, 2, 5, 1, 4, 4, 6, 1, 5,
        2, 5, 5, 3, 3, 5, 5, 6, 6
    };

    DirectSolveConfig config = make_direct_solve_config(
        SolverMode::Normal, 
        PatternPreset::None,
        false, false, false
    );

    SolveStats stats = {};
    
    // Execute the solver on the HARDCODED state, ignoring the visualizer
    bool ok = app_run_solve_from_state(test_scramble, config, &stats);

    if (ok) {
        std::cout << "[SUCCESS] Engine is working perfectly! Solution: " << stats.solution << std::endl;
        std::cout << "[CONCLUSION] The silent crash is 100% caused by the visualizer array generation mapping." << std::endl;
    } else {
        std::cout << "[ERROR] Engine failed to solve a known valid state." << std::endl;
    }
}
*/
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
            g_SceneInstance = nullptr;
            g_RubikInstance = nullptr;
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
            g_RubikInstance->parseAndQueueMove(shiftPressed ? "L'" : "L");
            g_MoveHistory.push_back(shiftPressed ? "L'" : "L");
            break;
        case GLFW_KEY_W:
            g_RubikInstance->parseAndQueueMove(shiftPressed ? "M'" : "M");
            g_MoveHistory.push_back(shiftPressed ? "M'" : "M");
            break;
        case GLFW_KEY_E:
            g_RubikInstance->parseAndQueueMove(shiftPressed ? "R'" : "R");
            g_MoveHistory.push_back(shiftPressed ? "R'" : "R");
            break;

        case GLFW_KEY_A:
            g_RubikInstance->parseAndQueueMove(shiftPressed ? "D'" : "D");
            g_MoveHistory.push_back(shiftPressed ? "D'" : "D");
            break;
        case GLFW_KEY_S:
            g_RubikInstance->parseAndQueueMove(shiftPressed ? "E'" : "E");
            g_MoveHistory.push_back(shiftPressed ? "E'" : "E");
            break;
        case GLFW_KEY_D:
            g_RubikInstance->parseAndQueueMove(shiftPressed ? "U'" : "U");
            g_MoveHistory.push_back(shiftPressed ? "U'" : "U");
            break;

        case GLFW_KEY_Z:
            g_RubikInstance->parseAndQueueMove(shiftPressed ? "F'" : "F");
            g_MoveHistory.push_back(shiftPressed ? "F'" : "F");
            break;
        case GLFW_KEY_X:
            g_RubikInstance->parseAndQueueMove(shiftPressed ? "S'" : "S");
            g_MoveHistory.push_back(shiftPressed ? "S'" : "S");
            break;
        case GLFW_KEY_C:
            g_RubikInstance->parseAndQueueMove(shiftPressed ? "B'" : "B");
            g_MoveHistory.push_back(shiftPressed ? "B'" : "B");
            break;

        case GLFW_KEY_P:
            g_RubikInstance->scramble(20);
            // NOTE: You must also update your RubikCube::scramble() function internally 
            // so that it pushes its 20 random moves into g_MoveHistory!
            break;

        case GLFW_KEY_L:
            uint8_t solverState[54];
            if (!g_RubikInstance->getSolverState(solverState)) {
        return; 
    }
            break;
            
        case GLFW_KEY_O: // The New History-Stack Solver
            if (!g_RubikInstance->isAnimating && g_RubikInstance->moveQueue.empty()) {
                
                if (g_MoveHistory.empty()) {
                    std::cout << "[SYSTEM] Cube is already at its starting state!" << std::endl;
                } else {
                    std::cout << "[SYSTEM] Initiating Reverse-History Solver (" << g_MoveHistory.size() << " moves)..." << std::endl;

                    // Read the stack backwards (from most recent move to the very first move)
                    for (auto it = g_MoveHistory.rbegin(); it != g_MoveHistory.rend(); ++it) {
                        std::string invMove = getInverseMove(*it);
                        g_RubikInstance->parseAndQueueMove(invMove);
                    }
                    
                    // Clear the history because the cube is now returning to a solved state
                    g_MoveHistory.clear();
                    
                    std::cout << "[SUCCESS] Reversal animations queued perfectly!" << std::endl;
                }
            } else {
                std::cout << "[SYSTEM] Solver rejected: Cube is currently animating or queue is busy." << std::endl;
            }
            break;

        case GLFW_KEY_ENTER:
            triggerZZOptimalSolver();
            g_MoveHistory.clear();
            break;
/*
        case GLFW_KEY_M:
            triggerZZOptimalSolver2();
            break;

        case GLFW_KEY_N:
            triggerZZOptimalSolver3();
            break;
*/        
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