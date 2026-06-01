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
#include <queue>

#define WIDTH 1000
#define HEIGHT 800

class Scene;
class RubikCube;
struct CubeMove;
//class RubikSolver;

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

    // Returns the index of the color palette (0-5) currently facing an absolute world direction vector
    myglm::vec4 getColorFacingWorldDirection(const myglm::vec3& targetWorldDir) {
        // Baseline untransformed face directions matching assignedColors[0..5] exactly
        myglm::vec3 originalFaceDirs[6] = {
            myglm::vec3(0.0f, 0.0f, -1.0f), // 0: Front (-Z)
            myglm::vec3(0.0f, 0.0f,  1.0f), // 1: Back (+Z)
            myglm::vec3(-1.0f, 0.0f, 0.0f), // 2: Left (-X)
            myglm::vec3( 1.0f, 0.0f, 0.0f), // 3: Right (+X)
            myglm::vec3(0.0f,  1.0f, 0.0f), // 4: Up (+Y)
            myglm::vec3(0.0f, -1.0f, 0.0f)  // 5: Down (-Y)
        };

        float bestDot = -2.0f;
        int matchingFaceIdx = 0;

        for (int i = 0; i < 6; i++) {
            // 1. Pack the face direction vector into a vec4 (w = 0.0f discards matrix translations)
            myglm::vec4 directionVec4(originalFaceDirs[i], 0.0f);
            
            // 2. Rotate the vector using the cubie's permanent orientation matrix
            myglm::vec4 transformedVec4 = homeMatrix * directionVec4;
            
            // 3. Explicitly extract components to avoid implicit vec4-to-vec3 conversion bugs
            myglm::vec3 currentWorldDirOfSticker(transformedVec4.x, transformedVec4.y, transformedVec4.z);
            
            // 4. Measure alignment with the requested world direction using a dot product
            float dotVal = myglm::dot(currentWorldDirOfSticker, targetWorldDir);
            if (dotVal > bestDot) {
                bestDot = dotVal;
                matchingFaceIdx = i;
            }
        }

        // Return the stable vec4 color data assigned to that specific physical side
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
    float rotationSpeed = 400.0f; 

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
                    if (i == 0) assignedColors[0] = palette[4]; // Front
                    if (i == 2) assignedColors[1] = palette[2]; // Back
                    if (k == 0) assignedColors[2] = palette[1]; // Left
                    if (k == 2) assignedColors[3] = palette[3]; // Right
                    if (j == 2) assignedColors[4] = palette[0]; // Up
                    if (j == 0) assignedColors[5] = palette[5]; // Down

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
        // A complete 27-move pool covering all outer layers and middle slices
        // across standard (90°), prime (-90°), and double (180°) variations.
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

        for (int i = 0; i < movesCount; i++) {
            // Updated modulo to match the new 27-element pool size
            int randIndex = std::rand() % 27;
            parseAndQueueMove(movePool[randIndex]);
        }
    }

    myglm::mat4 getActiveAnimationMatrix() {
        if (!isAnimating) return myglm::mat4(1.0f);
        return myglm::rotate(myglm::mat4(1.0f), myglm::radians(animationProgressAngle), currentAnimationAxis);
    }

    ~RubikCube() {
        for (Cubie* piece : cubies) delete piece;
    }
};
/*
class RubikSolver {
private:
    RubikCube* cube;

    struct VirtualCubie {
        Cubie* realCubie;
        int x, y, z;
        myglm::mat4 vMatrix;
    };

    std::vector<VirtualCubie> vCubies;

    VirtualCubie* getVirtualCubieAt(int x, int y, int z) {
        for (auto& vc : vCubies) {
            if (vc.x == x && vc.y == y && vc.z == z) return &vc;
        }
        return nullptr;
    }

    bool isColorEqual(const myglm::vec4& c1, const myglm::vec4& c2) {
        return (std::abs(c1.x - c2.x) < 0.01f &&
                std::abs(c1.y - c2.y) < 0.01f &&
                std::abs(c1.z - c2.z) < 0.01f);
    }

    myglm::vec4 getVirtualColorFacing(const VirtualCubie& vc, const myglm::vec3& targetWorldDir) {
        myglm::vec3 originalFaceDirs[6] = {
            myglm::vec3(0.0f, 0.0f, -1.0f), // 0: Front (-Z)
            myglm::vec3(0.0f, 0.0f,  1.0f), // 1: Back (+Z)
            myglm::vec3(-1.0f, 0.0f, 0.0f), // 2: Left (-X)
            myglm::vec3( 1.0f, 0.0f, 0.0f), // 3: Right (+X)
            myglm::vec3(0.0f,  1.0f, 0.0f), // 4: Up (+Y)
            myglm::vec3(0.0f, -1.0f, 0.0f)  // 5: Down (-Y)
        };

        float bestDot = -2.0f;
        int matchingFaceIdx = 0;

        for (int i = 0; i < 6; i++) {
            myglm::vec4 directionVec4(originalFaceDirs[i], 0.0f);
            myglm::vec4 transformedVec4 = vc.vMatrix * directionVec4;
            myglm::vec3 currentWorldDir(transformedVec4.x, transformedVec4.y, transformedVec4.z);
            
            float dotVal = myglm::dot(currentWorldDir, targetWorldDir);
            if (dotVal > bestDot) {
                bestDot = dotVal;
                matchingFaceIdx = i;
            }
        }
        return vc.realCubie->faceColors[matchingFaceIdx];
    }

    void executeMove(const std::string& moveStr) {
        cube->parseAndQueueMove(moveStr); 

        char face = moveStr[0];
        bool isPrime = (moveStr.size() > 1 && moveStr[1] == '\'');
        bool isDouble = (moveStr.size() > 1 && moveStr[1] == '2');
        float baseAngle = 90.0f;
        if (isPrime) baseAngle = -90.0f;
        if (isDouble) baseAngle = 180.0f;

        myglm::vec3 axis(0.0f);
        float angle = 0.0f;
        int targetLayer = 0; 
        int axisType = 0; 

        switch (face) {
            case 'U': axis = myglm::vec3(0.0f, 1.0f, 0.0f);  angle = -baseAngle; targetLayer = 1;  axisType = 1; break;
            case 'D': axis = myglm::vec3(0.0f, 1.0f, 0.0f);  angle = baseAngle;  targetLayer = -1; axisType = 1; break;
            case 'R': axis = myglm::vec3(1.0f, 0.0f, 0.0f);  angle = -baseAngle; targetLayer = 1;  axisType = 0; break;
            case 'L': axis = myglm::vec3(1.0f, 0.0f, 0.0f);  angle = baseAngle;  targetLayer = -1; axisType = 0; break;
            case 'F': axis = myglm::vec3(0.0f, 0.0f, 1.0f);  angle = -baseAngle; targetLayer = 1;  axisType = 2; break; // Z = 1 Front
            case 'B': axis = myglm::vec3(0.0f, 0.0f, 1.0f);  angle = baseAngle;  targetLayer = -1; axisType = 2; break; // Z = -1 Back
        }

        myglm::mat4 rotMatrix = myglm::rotate(myglm::mat4(1.0f), myglm::radians(angle), axis);

        for (auto& vc : vCubies) {
            bool inActiveSlice = false;
            if (axisType == 0 && vc.x == targetLayer) inActiveSlice = true;
            if (axisType == 1 && vc.y == targetLayer) inActiveSlice = true;
            if (axisType == 2 && vc.z == targetLayer) inActiveSlice = true;

            if (inActiveSlice) {
                vc.vMatrix = rotMatrix * vc.vMatrix;
                myglm::vec4 p(static_cast<float>(vc.x), static_cast<float>(vc.y), static_cast<float>(vc.z), 1.0f);
                myglm::vec4 updatedP = rotMatrix * p;
                vc.x = static_cast<int>(std::round(updatedP.x));
                vc.y = static_cast<int>(std::round(updatedP.y));
                vc.z = static_cast<int>(std::round(updatedP.z));
            }
        }
    }

public:
    RubikSolver(RubikCube* targetCube) : cube(targetCube) {
        for (Cubie* c : cube->cubies) {
            VirtualCubie vc;
            vc.realCubie = c;
            vc.x = c->gridX;
            vc.y = c->gridY;
            vc.z = c->gridZ;
            vc.vMatrix = c->homeMatrix;
            vCubies.push_back(vc);
        }
    }

    void solve() {
        std::cout << "[SOLVER] Running dynamic logic pathfinder..." << std::endl;
        
        myglm::vec4 white(1.0f, 1.0f, 1.0f, 1.0f);
        myglm::vec4 blue(0.0f, 0.0f, 1.0f, 1.0f);    // Front Face (Z = 1)
        myglm::vec4 red(1.0f, 0.0f, 0.0f, 1.0f);     // Right Face (X = 1)
        myglm::vec4 green(0.0f, 1.0f, 0.0f, 1.0f);   // Back Face (Z = -1)
        myglm::vec4 orange(1.0f, 0.5f, 0.0f, 1.0f);  // Left Face (X = -1)

        // Run the complete Phase 1 sequence for all 4 cross edges seamlessly
        solveWhiteEdgeWithCenter(white, blue,   0,  1, "F");
        solveWhiteEdgeWithCenter(white, red,    1,  0, "R");
        solveWhiteEdgeWithCenter(white, green,  0, -1, "B");
        solveWhiteEdgeWithCenter(white, orange, -1,  0, "L");

        std::cout << "[SOLVER] Move generation sequence complete." << std::endl;
    }

    void solveWhiteEdgeWithCenter(const myglm::vec4& white, const myglm::vec4& sideColor, int targetX, int targetZ, const std::string& faceNotation) {
        VirtualCubie* targetEdge = nullptr;

        // 1. FIXED: Added a strict 'nonBlackCount == 2' check to ignore corner pieces completely
        for (auto& vc : vCubies) {
            bool hasWhite = false, hasSideColor = false;
            int nonBlackStickers = 0;
            for (int f = 0; f < 6; f++) {
                myglm::vec4 c = vc.realCubie->faceColors[f];
                if (c.x > 0.16f || c.y > 0.16f || c.z > 0.16f) { 
                    nonBlackStickers++;
                    if (isColorEqual(c, white)) hasWhite = true;
                    if (isColorEqual(c, sideColor)) hasSideColor = true;
                }
            }
            if (hasWhite && hasSideColor && nonBlackStickers == 2) {
                targetEdge = &vc;
                break;
            }
        }

        if (!targetEdge) return;

        // 2. Check if the piece is ALREADY perfectly solved on the Top layer
        if (targetEdge->x == targetX && targetEdge->y == 1 && targetEdge->z == targetZ) {
            myglm::vec4 currentTopColor = getVirtualColorFacing(*targetEdge, myglm::vec3(0.0f, 1.0f, 0.0f));
            if (isColorEqual(currentTopColor, white)) {
                std::cout << "[SOLVER] Piece (" << faceNotation << ") is already solved. Skipping." << std::endl;
                return; 
            }
        }

        // 3. Bring the piece safely down to the bottom layer workspace setup (y == -1)
        if (targetEdge->y == 1) { 
            if (targetEdge->z == 1)       { executeMove("F2"); }
            else if (targetEdge->x == 1)  { executeMove("R2"); }
            else if (targetEdge->z == -1) { executeMove("B2"); }
            else if (targetEdge->x == -1) { executeMove("L2"); }
        }
        else if (targetEdge->y == 0) { 
            if (targetEdge->x == 1 && targetEdge->z == 1)        { executeMove("R'"); executeMove("D"); executeMove("R"); }
            else if (targetEdge->x == -1 && targetEdge->z == 1)  { executeMove("L");  executeMove("D'"); executeMove("L'"); }
            else if (targetEdge->x == 1 && targetEdge->z == -1)  { executeMove("R");  executeMove("D'"); executeMove("R'"); }
            else if (targetEdge->x == -1 && targetEdge->z == -1) { executeMove("L'"); executeMove("D"); executeMove("L"); }
        }

        // 4. Rotate the bottom layer (D) until it sits directly underneath its home column
        while (true) {
            if (targetX == 0 && targetZ == 1  && targetEdge->z == 1)  break; // Front Column
            if (targetX == 1 && targetZ == 0  && targetEdge->x == 1)  break; // Right Column
            if (targetX == 0 && targetZ == -1 && targetEdge->z == -1) break; // Back Column
            if (targetX == -1 && targetZ == 0 && targetEdge->x == -1) break; // Left Column
            executeMove("D"); 
        }

        // 5. Inspect orientation from the bottom view perspective
        myglm::vec4 facingDownColor = getVirtualColorFacing(*targetEdge, myglm::vec3(0.0f, -1.0f, 0.0f));
        
        if (isColorEqual(facingDownColor, white)) {
            // Case A: White faces down. A simple 180° turn puts it perfectly in place on top!
            executeMove(faceNotation + "2");
        } else {
            // Case B: FIXED. Symmetry-aligned 4-move flipper that protects adjacent cross slots
            if (faceNotation == "F") { executeMove("D'"); executeMove("R'"); executeMove("F"); executeMove("R"); }
            else if (faceNotation == "R") { executeMove("D'"); executeMove("B'"); executeMove("R"); executeMove("B"); }
            else if (faceNotation == "B") { executeMove("D'"); executeMove("L'"); executeMove("B"); executeMove("L"); }
            else if (faceNotation == "L") { executeMove("D'"); executeMove("F'"); executeMove("L"); executeMove("F"); }
        }
    }
};
*/
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

        case GLFW_KEY_P:
            g_RubikInstance->scramble(20);
            break;
        // case GLFW_KEY_O:
        //     if (!g_RubikInstance->isAnimating && g_RubikInstance->moveQueue.empty()) {
        //         RubikSolver solver(g_RubikInstance);
        //         solver.solve();
        //     } else {
        //         std::cout << "[SYSTEM] Solver rejected: Cube is currently animating or queue is busy." << std::endl;
        //     }
        //     break;

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