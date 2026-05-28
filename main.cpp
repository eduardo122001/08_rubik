// cambios realizados por edcr se pondran como 'edcambio'

#define GLAD_GL_IMPLEMENTATION
#include <glad/gl.h>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include "myglm.h"

#include "linmath.h"

#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>
#include <vector>
#include <iostream>
#include <string>

#include <random>

#define WIDTH 1000
#define HEIGHT 800

const char *vertexShaderSource = "#version 330 core\n"
								 "layout (location = 0) in vec3 aPos;\n"
								 "uniform mat4 model;\n"
								 "uniform mat4 view;\n"
								 "uniform mat4 projection;\n"
								 "void main()\n"
								 "{\n"
								 "   gl_Position = projection*view*model*vec4(aPos, 1.0);\n"
								 "}\0";

const char *fragmentShaderSourceUniform = "#version 330 core\n"
										  "out vec4 FragColor;\n"
										  "uniform vec4 color;\n"
										  "void main()\n"
										  "{\n"
										  "   FragColor = color;\n"
										  "}\n\0";

const char *fragmentShaderSource1 = "#version 330 core\n"
									"out vec4 FragColor;\n"
									"void main()\n"
									"{\n"
									"   FragColor = vec4(0.0f, 1.0f, 0.0f, 1.0f);\n"
									"}\n\0";

const char *fragmentShaderSource2 = "#version 330 core\n"
									"out vec4 FragColor;\n"
									"void main()\n"
									"{\n"
									"   FragColor = vec4(1.0f, 0.0f, 0.0f, 1.0f);\n"
									"}\n\0";

const char *fragmentShaderSource3 = "#version 330 core\n"
									"out vec4 FragColor;\n"
									"void main()\n"
									"{\n"
									"   FragColor = vec4(1.0f, 1.0f, 0.0f, 1.0f);\n"
									"}\n\0";

float getRadiusForNEdges(unsigned int nEdges, float edgeLenght = 1.0f)
{
	return edgeLenght / 2 / cos(myglm::radians(90) * (1 - 2 / (float)nEdges));
}

// Classes

class Shape
{
public:
	unsigned int VBO, VAO, EBO, EBOedges;

	unsigned int lastColorLoc = 0, lastModelLoc = 0;

	unsigned int lastShaderProgram = 0;

	bool isVisible = true;

	bool rotateAroundOrigin = false;

	myglm::vec3 translation = myglm::vec3(0.0f, 0.0f, 0.0f);
	myglm::vec3 rotation = myglm::vec3(0.0f, 0.0f, 0.0f);
	myglm::vec3 scale = myglm::vec3(1.0f, 1.0f, 1.0f);
	std::vector<myglm::vec3> vertices;
	std::vector<unsigned int> indices;
	std::vector<unsigned int> indicesEdges;

	bool isDirty = true;
	myglm::mat4 lastModel = myglm::mat4(1.0f);
	void swapVisibility()
	{
		isVisible = !isVisible;
	}

	void displayState()
	{
		std::cout << "Position: " << translation.x << " " << translation.y << " " << translation.z << "\n"
				  << "Rotation: " << rotation.x << " " << rotation.y << " " << rotation.z << "\n"
				  << "Scale: " << scale.x << " " << scale.y << " " << scale.z << "\n\n";
	}

	void swapRotationState()
	{
		rotateAroundOrigin = !rotateAroundOrigin;
	}

	void drawVertices()
	{
		myglm::vec4 color(0.0f, 0.0f, 0.0f, 1.0f);
		glUniform4fv(lastColorLoc, 1, myglm::value_ptr(color));
		glDrawArrays(GL_POINTS, 0, vertices.size());
	}
	void draw(unsigned int shaderProgram)
	{
		if (!isVisible)
			return;
		if (lastShaderProgram != shaderProgram)
		{
			glUseProgram(shaderProgram);
			lastShaderProgram = shaderProgram;
			lastModelLoc = glGetUniformLocation(shaderProgram, "model");
			lastColorLoc = glGetUniformLocation(shaderProgram, "color");
		}
		glUniformMatrix4fv(lastModelLoc, 1, GL_FALSE, myglm::value_ptr(getTranformMatrix(rotateAroundOrigin)));
		glBindVertexArray(VAO);
		drawShape();
		drawEdges();
		drawVertices();
	}

	virtual void drawEdges() = 0;
	virtual void drawShape() = 0;

	virtual void generateVertices(myglm::vec3 position, myglm::vec3 scale, myglm::vec3 rotation) = 0;
	virtual void generateIndices() = 0;
	virtual void generateIndicesEdges() = 0;

	void initialize(myglm::vec3 position, myglm::vec3 scale, myglm::vec3 rotation)
	{

		generateVertices(position, scale, rotation);
		generateIndices();
		generateIndicesEdges();

		glGenVertexArrays(1, &VAO);
		glGenBuffers(1, &VBO);
		glGenBuffers(1, &EBO);
		glGenBuffers(1, &EBOedges);
		glBindVertexArray(VAO);
		glBindBuffer(GL_ARRAY_BUFFER, VBO);
		glBufferData(GL_ARRAY_BUFFER, sizeof(myglm::vec3) * vertices.size(), vertices.data(), GL_STATIC_DRAW);

		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(unsigned int) * indices.size(), indices.data(), GL_STATIC_DRAW);

		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBOedges);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(unsigned int) * indicesEdges.size(), indicesEdges.data(), GL_STATIC_DRAW);

		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(myglm::vec3), (void *)0);
		glEnableVertexAttribArray(0);

		glBindBuffer(GL_ARRAY_BUFFER, 0);

		glBindVertexArray(0);
	}

	void setPosition(myglm::vec3 newPos)
	{
		translation = newPos;
		isDirty = true;
	}
	void translate(myglm::vec3 offset)
	{
		translation += offset;
		isDirty = true;
	}
	void translateInverse(myglm::vec3 offset)
	{
		translation -= offset;
		isDirty = true;
	}

	void setRotation(myglm::vec3 newRot)
	{
		rotation = newRot;
		isDirty = true;
	}
	void rotate(myglm::vec3 offsetRot)
	{
		rotation += offsetRot;
		isDirty = true;
	}
	void rotateInverse(myglm::vec3 offsetRot)
	{
		rotation -= offsetRot;
		isDirty = true;
	}

	void setScale(myglm::vec3 newScale)
	{
		scale = newScale;
		isDirty = true;
	}
	void scaleBy(myglm::vec3 multiplier)
	{
		scale *= multiplier;
		isDirty = true;
	}
	void scaleByInverse(myglm::vec3 multiplier)
	{
		scale /= multiplier;
		isDirty = true;
	}

	void setScale(float newScale)
	{
		scale = myglm::vec3(newScale);
		isDirty = true;
	}
	void scaleBy(float multiplier)
	{
		scale *= multiplier;
		isDirty = true;
	}
	void scaleByInverse(float multiplier)
	{
		scale /= multiplier;
		isDirty = true;
	}

	const myglm::mat4 &getTranformMatrix(bool rotateAroundOrigin)
	{
		if (isDirty)
		{
			if (rotateAroundOrigin)
			{
				lastModel = myglm::mat4(1.0f);

				lastModel = myglm::rotate(lastModel, rotation.x, myglm::vec3(1.0f, 0.0f, 0.0f));
				lastModel = myglm::rotate(lastModel, rotation.y, myglm::vec3(0.0f, 1.0f, 0.0f));
				lastModel = myglm::rotate(lastModel, rotation.z, myglm::vec3(0.0f, 0.0f, 1.0f));

				lastModel = myglm::translate(lastModel, translation);

				lastModel = myglm::scale(lastModel, scale);

				isDirty = false;
			}
			else
			{
				lastModel = myglm::mat4(1.0f);
				lastModel = myglm::translate(lastModel, translation);

				lastModel = myglm::rotate(lastModel, rotation.x, myglm::vec3(1.0f, 0.0f, 0.0f));
				lastModel = myglm::rotate(lastModel, rotation.y, myglm::vec3(0.0f, 1.0f, 0.0f));
				lastModel = myglm::rotate(lastModel, rotation.z, myglm::vec3(0.0f, 0.0f, 1.0f));

				lastModel = myglm::scale(lastModel, scale);

				isDirty = false;
			}
		}
		return lastModel;
	}

	virtual ~Shape()
	{
		glDeleteVertexArrays(1, &VAO);
		glDeleteBuffers(1, &VBO);
		glDeleteBuffers(1, &EBO);
		glDeleteBuffers(1, &EBOedges);
	}
};
class Cube : public Shape
{
public:
	Cube(myglm::vec3 position, myglm::vec3 scale, myglm::vec3 rotation)
	{
		initialize(position, scale, rotation);
	}

	void generateVertices(myglm::vec3 position, myglm::vec3 scale, myglm::vec3 rotation)
	{

		vertices = {
			myglm::vec3(-0.5f, -0.5f, -0.5f), // left down front
			myglm::vec3(-0.5f, -0.5f, 0.5f),  // left down back
			myglm::vec3(0.5f, -0.5f, -0.5f),  // right down front
			myglm::vec3(0.5f, -0.5f, 0.5f),	  // right down back
			myglm::vec3(-0.5f, 0.5f, -0.5f),  // left up front
			myglm::vec3(-0.5f, 0.5f, 0.5f),	  // left up back
			myglm::vec3(0.5f, 0.5f, -0.5f),	  // right up front
			myglm::vec3(0.5f, 0.5f, 0.5f)	  // right up back
		};

		setPosition(position);
		setRotation(rotation);
		setScale(scale);
	}
	void generateIndices()
	{

		indices = {
			0, 2, 6, 0, 6, 4, // front
			1, 5, 7, 1, 7, 3, // back
			1, 0, 4, 1, 4, 5, // left
			2, 3, 7, 2, 7, 6, // right
			4, 6, 7, 4, 7, 5, // up
			1, 3, 2, 1, 2, 0  // down
		};
	}
	void generateIndicesEdges()
	{

		indicesEdges = {
			0, 1, // bottom
			1, 3,
			0, 2,
			3, 2,

			0, 4, // sides
			1, 5,
			2, 6,
			3, 7,

			4, 5, // top
			6, 7,
			5, 7,
			4, 6};
	}

	void drawEdges()
	{
		myglm::vec4 color(1.0f, 1.0f, 1.0f, 1.0f);
		glUniform4fv(lastColorLoc, 1, myglm::value_ptr(color));
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBOedges);
		glDrawElements(GL_LINES, 24, GL_UNSIGNED_INT, 0);
	}
	void drawShape()
	{
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);

		std::vector<myglm::vec4> color = {myglm::vec4(1.0f, 0.0f, 0.0f, 1.0f),
										  myglm::vec4(0.0f, 1.0f, 0.0f, 1.0f),
										  myglm::vec4(0.0f, 0.0f, 1.0f, 1.0f),
										  myglm::vec4(1.0f, 1.0f, 0.0f, 1.0f),
										  myglm::vec4(1.0f, 0.0f, 1.0f, 1.0f),
										  myglm::vec4(0.0f, 1.0f, 1.0f, 1.0f)};

		for (int i = 0; i < 6; i++)
		{
			glUniform4fv(lastColorLoc, 1, myglm::value_ptr(color[i]));
			glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, (void *)(6 * i * sizeof(unsigned int)));
		}
	}
};

class SingleRubikCube : public Shape
{
	// int type;	//1 middle cube (1 color) , 2 side cube (2 colors), 3 corner cube (3 colors)

	myglm::vec4 faceColors[6];

public:
	SingleRubikCube(myglm::vec3 position, myglm::vec3 scale, myglm::vec3 rotation, std::vector<myglm::vec4> &colorsArray)
	{

		initialize(position, scale, rotation);

		for (int f = 0; f < 6; f++)
		{
			faceColors[f] = colorsArray[f];
		}
	}

	void generateVertices(myglm::vec3 position, myglm::vec3 scale, myglm::vec3 rotation)
	{

		vertices = {
			myglm::vec3(-0.5f, -0.5f, -0.5f), // left down back
			myglm::vec3(-0.5f, -0.5f, 0.5f),  // left down front
			myglm::vec3(0.5f, -0.5f, -0.5f),  // right down back
			myglm::vec3(0.5f, -0.5f, 0.5f),	  // right down front
			myglm::vec3(-0.5f, 0.5f, -0.5f),  // left up back
			myglm::vec3(-0.5f, 0.5f, 0.5f),	  // left up front
			myglm::vec3(0.5f, 0.5f, -0.5f),	  // right up back
			myglm::vec3(0.5f, 0.5f, 0.5f)	  // right up front
		};

		setPosition(position);
		setRotation(rotation);
		setScale(scale);
	}
	void generateIndices()
	{

		indices = {
			0, 2, 6, 0, 6, 4, // front
			1, 5, 7, 1, 7, 3, // back
			1, 0, 4, 1, 4, 5, // left
			2, 3, 7, 2, 7, 6, // right
			4, 6, 7, 4, 7, 5, // up
			1, 3, 2, 1, 2, 0  // down
		};
	}
	void generateIndicesEdges()
	{

		indicesEdges = {
			0, 1, // bottom
			1, 3,
			0, 2,
			3, 2,

			0, 4, // sides
			1, 5,
			2, 6,
			3, 7,

			4, 5, // top
			6, 7,
			5, 7,
			4, 6};
	}

	void drawEdges()
	{
		myglm::vec4 color(1.0f, 1.0f, 1.0f, 1.0f);
		glUniform4fv(lastColorLoc, 1, myglm::value_ptr(color));
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBOedges);
		glDrawElements(GL_LINES, 24, GL_UNSIGNED_INT, 0);
	}
	void drawShape()
	{
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);

		for (int f = 0; f < 6; f++)
		{
			glUniform4fv(lastColorLoc, 1, myglm::value_ptr(faceColors[f]));
			glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, (void *)(6 * f * sizeof(unsigned int)));
		}
	}
};

/*
class Rubik{
	public:
	std::vector<SingleRubikCube*>front;
	std::vector<SingleRubikCube*>back;
	std::vector<SingleRubikCube*>left;
	std::vector<SingleRubikCube*>rigth;
	std::vector<SingleRubikCube*>top;
	std::vector<SingleRubikCube*>bottom;


	;// = 1.0f;


	std::vector<SingleRubikCube*> cubes;

	Rubik(myglm::vec3 scale,myglm::vec3 rotation,myglm::vec3 _position,float sizeRubik  ){
	float sizeSingleCube=sizeRubik/3;
	float offSetPos = sizeSingleCube;

	myglm::vec3 scale(sizeSingleCube, sizeSingleCube, sizeSingleCube);
	myglm::vec3 rotation(0.0f, 0.0f, 0.0f);

	std::vector<myglm::vec4> colors = {
		myglm::vec4(1.0f, 1.0f, 1.0f, 1.0f), // White
		myglm::vec4(1.0f, 0.5f, 0.0f, 1.0f), // Orange
		myglm::vec4(0.0f, 0.0f, 1.0f, 1.0f), // Blue
		myglm::vec4(1.0f, 0.0f, 0.0f, 1.0f), // Red
		myglm::vec4(0.0f, 1.0f, 0.0f, 1.0f), // Green
		myglm::vec4(1.0f, 1.0f, 0.0f, 1.0f)  // Yellow
	};

	myglm::vec4 gray(0.5f, 0.5f, 0.5f, 1.0f);


	for (int i = 0; i<3 ; i++){
		for (int j  = 0 ; j<3; j++){
			for (int k  = 0 ; k<3; k++){

				myglm::vec3 position(
					_position.x + sizeSingleCube * k - offSetPos,
					_position.y + sizeSingleCube * j - offSetPos,
					_position.z + sizeSingleCube * i - offSetPos
				);

				std::vector<myglm::vec4> fColors(6, gray);

				if (i == 0) fColors[0] = myglm::vec4(colors[4]); // Front face (-Z) gets Green
				if (i == 2) fColors[1] = myglm::vec4(colors[2]); // Back face (+Z) gets Blue
				if (k == 0) fColors[2] = myglm::vec4(colors[1]); // Left face (-X) gets Orange
				if (k == 2) fColors[3] = myglm::vec4(colors[3]); // Right face (+X) gets Red
				if (j == 2) fColors[4] = myglm::vec4(colors[0]); // Up face (+Y) gets White
				if (j == 0) fColors[5] = myglm::vec4(colors[5]); // Down face (-Y) gets Yellow

				cubes.push_back(new SingleRubikCube(position, scale, rotation,fColors));
				//myScene.shapePointers.push_back(cubes[cubes.size()-1]);
			}
		}
	}


	}
};
*/

class Pyramid : public Shape
{
public:
	unsigned int nBaseEdges;
	float height;
	float radio;
	Pyramid(myglm::vec3 position, myglm::vec3 scale, myglm::vec3 rotation, float radio = 0.5f, float height = 0.5f, unsigned int nBaseEdges = 3)
	{
		this->nBaseEdges = nBaseEdges;
		this->height = height;
		this->radio = radio;
		initialize(position, scale, rotation);
	}
	void generateBase()
	{
		myglm::vec3 center(0.0f, 0.0f, -height / 2);
		vertices.push_back(center);
		float currentAngle;
		float anglePerEdge = (2 * M_PI) / nBaseEdges;

		for (int i = 0; i <= nBaseEdges; i++)
		{
			currentAngle = i * anglePerEdge;
			float x = radio * cos(currentAngle);
			float y = radio * sin(currentAngle);
			vertices.push_back(myglm::vec3(x, y, -height / 2));
		}
	}
	void generateVertices(myglm::vec3 position, myglm::vec3 scale, myglm::vec3 rotation)
	{
		generateBase();
		vertices.push_back(myglm::vec3(0.0f, 0.0f, height / 2));

		setPosition(position);
		setRotation(rotation);
		setScale(scale);
	}
	void generateIndices()
	{
		unsigned int top = vertices.size() - 1;
		for (int i = 1; i <= nBaseEdges; i++)
		{
			indices.push_back(top);
			indices.push_back(i);
			indices.push_back(i + 1);
		}
	}
	void generateIndicesEdges()
	{
		unsigned int top = vertices.size() - 1;
		// base edges
		for (int i = 1; i <= nBaseEdges; i++)
		{
			// Base edge
			indicesEdges.push_back(i);
			indicesEdges.push_back(i + 1);

			// Lateral edge
			indicesEdges.push_back(i);
			indicesEdges.push_back(top);
		}
	}

	void drawEdges()
	{
		myglm::vec4 color(1.0f, 1.0f, 1.0f, 1.0f);
		glUniform4fv(lastColorLoc, 1, myglm::value_ptr(color));
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBOedges);
		glDrawElements(GL_LINES, indicesEdges.size(), GL_UNSIGNED_INT, 0);
	}
	void drawShape()
	{
		std::vector<myglm::vec4> color = {myglm::vec4(1.0f, 0.0f, 0.0f, 1.0f),
										  myglm::vec4(0.0f, 1.0f, 0.0f, 1.0f),
										  myglm::vec4(0.0f, 0.0f, 1.0f, 1.0f),
										  myglm::vec4(1.0f, 1.0f, 0.0f, 1.0f),
										  myglm::vec4(1.0f, 0.0f, 1.0f, 1.0f),
										  myglm::vec4(0.0f, 1.0f, 1.0f, 1.0f)};

		glUniform4fv(lastColorLoc, 1, myglm::value_ptr(color[0]));
		glDrawArrays(GL_TRIANGLE_FAN, 0, nBaseEdges + 2);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);

		for (int i = 0; i < nBaseEdges; i++)
		{
			glUniform4fv(lastColorLoc, 1, myglm::value_ptr(color[(i + 1) % 6]));
			glDrawElements(GL_TRIANGLES, 3, GL_UNSIGNED_INT, (void *)(3 * i * sizeof(unsigned int)));
		}
	}
};

class Prism : public Shape
{
public:
	unsigned int nBaseEdges;
	float height;
	float radio;
	Prism(myglm::vec3 position, myglm::vec3 scale, myglm::vec3 rotation, float radio = 0.5f, float height = 0.5f, unsigned int nBaseEdges = 3)
	{
		this->nBaseEdges = nBaseEdges;
		this->height = height;
		this->radio = radio;
		initialize(position, scale, rotation);
	}
	void generateBases()
	{
		myglm::vec3 centerBot(0.0f, 0.0f, height / 2);
		myglm::vec3 centerTop(0.0f, 0.0f, -height / 2);
		vertices.push_back(centerBot);
		float currentAngle;
		float anglePerEdge = (2 * M_PI) / nBaseEdges;

		for (int i = 0; i <= nBaseEdges; i++)
		{
			currentAngle = i * anglePerEdge;
			float x = radio * cos(currentAngle);
			float y = radio * sin(currentAngle);
			vertices.push_back(myglm::vec3(x, y, height / 2));
		}

		vertices.push_back(centerTop);
		for (int i = 0; i <= nBaseEdges; i++)
		{
			currentAngle = i * anglePerEdge;
			float x = radio * cos(currentAngle);
			float y = radio * sin(currentAngle);
			vertices.push_back(myglm::vec3(x, y, -height / 2));
		}
	}
	void generateVertices(myglm::vec3 position, myglm::vec3 scale, myglm::vec3 rotation)
	{
		generateBases();

		setPosition(position);
		setRotation(rotation);
		setScale(scale);
	}
	void generateIndices()
	{
		for (int i = 1; i <= nBaseEdges; i++)
		{
			unsigned int bot = i;
			unsigned int top = i + nBaseEdges + 2;

			// First triangle of the quad
			indices.push_back(bot);
			indices.push_back(bot + 1);
			indices.push_back(top);

			// Second triangle of the quad
			indices.push_back(top + 1);
			indices.push_back(bot + 1);
			indices.push_back(top);
		}
	}
	void generateIndicesEdges()
	{
		unsigned int top = vertices.size() - 1;
		for (int i = 1; i <= nBaseEdges; i++)
		{
			unsigned int top = i + nBaseEdges + 2;
			unsigned int bot = i;

			// Top base edge
			indicesEdges.push_back(top);
			indicesEdges.push_back(top + 1);

			// Bottom base edge
			indicesEdges.push_back(bot);
			indicesEdges.push_back(bot + 1);

			// Lateral (vertical) edge
			indicesEdges.push_back(bot);
			indicesEdges.push_back(top);
		}
	}

	void drawEdges()
	{
		myglm::vec4 color(1.0f, 1.0f, 1.0f, 1.0f);
		glUniform4fv(lastColorLoc, 1, myglm::value_ptr(color));
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBOedges);
		glDrawElements(GL_LINES, indicesEdges.size(), GL_UNSIGNED_INT, 0);
	}
	void drawShape()
	{
		std::vector<myglm::vec4> color = {myglm::vec4(1.0f, 0.0f, 0.0f, 1.0f),
										  myglm::vec4(0.0f, 1.0f, 0.0f, 1.0f),
										  myglm::vec4(0.0f, 0.0f, 1.0f, 1.0f),
										  myglm::vec4(1.0f, 1.0f, 0.0f, 1.0f),
										  myglm::vec4(1.0f, 0.0f, 1.0f, 1.0f),
										  myglm::vec4(0.0f, 1.0f, 1.0f, 1.0f)};

		glUniform4fv(lastColorLoc, 1, myglm::value_ptr(color[0]));
		glDrawArrays(GL_TRIANGLE_FAN, 0, nBaseEdges + 2);
		glUniform4fv(lastColorLoc, 1, myglm::value_ptr(color[1]));
		glDrawArrays(GL_TRIANGLE_FAN, nBaseEdges + 2, nBaseEdges + 2);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);

		for (int i = 0; i < nBaseEdges; i++)
		{
			glUniform4fv(lastColorLoc, 1, myglm::value_ptr(color[(i + 2) % 6]));
			glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, (void *)(6 * i * sizeof(unsigned int)));
		}
	}
};

class Sphere : public Shape
{
public:
	float radius;
	unsigned int sectorCount; // Longitude (slices)
	unsigned int stackCount;  // Latitude (rings)

	Sphere(myglm::vec3 position, myglm::vec3 scale, myglm::vec3 rotation, float radius = 0.5f, unsigned int sectors = 18, unsigned int stacks = 9)
	{
		this->radius = radius;
		this->sectorCount = sectors;
		this->stackCount = stacks;
		initialize(position, scale, rotation);
	}

	void generateVertices(myglm::vec3 position, myglm::vec3 scale, myglm::vec3 rotation)
	{
		float x, y, z, xy;
		float sectorStep = 2 * M_PI / sectorCount;
		float stackStep = M_PI / stackCount;
		float sectorAngle, stackAngle;

		// Generate vertices from top to bottom
		for (int i = 0; i <= stackCount; i++)
		{
			stackAngle = M_PI / 2 - i * stackStep; // From pi/2 to -pi/2
			xy = radius * cosf(stackAngle);		   // r * cos(u)
			z = radius * sinf(stackAngle);		   // r * sin(u)

			for (int j = 0; j <= sectorCount; ++j)
			{
				sectorAngle = j * sectorStep; // From 0 to 2pi
				x = xy * cosf(sectorAngle);	  // r * cos(u) * cos(v)
				y = xy * sinf(sectorAngle);	  // r * cos(u) * sin(v)

				vertices.push_back(myglm::vec3(x, y, z));
			}
		}

		setPosition(position);
		setRotation(rotation);
		setScale(scale);
	}

	void generateIndices()
	{
		unsigned int k1, k2;
		for (int i = 0; i < stackCount; ++i)
		{
			k1 = i * (sectorCount + 1); // Beginning of current stack
			k2 = k1 + sectorCount + 1;	// Beginning of next stack

			for (int j = 0; j < sectorCount; ++j, ++k1, ++k2)
			{
				// 2 triangles per sector excluding first and last stacks
				if (i != 0)
				{
					indices.push_back(k1);
					indices.push_back(k2);
					indices.push_back(k1 + 1);
				}
				if (i != (stackCount - 1))
				{
					indices.push_back(k1 + 1);
					indices.push_back(k2);
					indices.push_back(k2 + 1);
				}
			}
		}
	}

	void generateIndicesEdges()
	{
		unsigned int k1, k2;
		for (int i = 0; i < stackCount; ++i)
		{
			k1 = i * (sectorCount + 1);
			k2 = k1 + sectorCount + 1;

			for (int j = 0; j < sectorCount; ++j, ++k1, ++k2)
			{
				// Horizontal line
				indicesEdges.push_back(k1);
				indicesEdges.push_back(k1 + 1);

				// Vertical line (avoid drawing redundant lines at the bottom pole)
				if (i != (stackCount - 1))
				{
					indicesEdges.push_back(k1);
					indicesEdges.push_back(k2);
				}
			}
		}
	}

	void drawEdges()
	{
		myglm::vec4 color(1.0f, 1.0f, 1.0f, 1.0f);
		glUniform4fv(lastColorLoc, 1, myglm::value_ptr(color));

		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBOedges);
		glDrawElements(GL_LINES, indicesEdges.size(), GL_UNSIGNED_INT, 0);
	}

	void drawShape()
	{
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);

		std::vector<myglm::vec4> color = {
			myglm::vec4(1.0f, 0.0f, 0.0f, 1.0f), // Red
			myglm::vec4(0.0f, 1.0f, 0.0f, 1.0f), // Green
			myglm::vec4(0.0f, 0.0f, 1.0f, 1.0f), // Blue
			myglm::vec4(1.0f, 1.0f, 0.0f, 1.0f), // Yellow
			myglm::vec4(1.0f, 0.0f, 1.0f, 1.0f), // Magenta
			myglm::vec4(0.0f, 1.0f, 1.0f, 1.0f)	 // Cyan
		};

		unsigned int indexOffset = 0;
		unsigned int indicesPerStack;

		for (int i = 0; i < stackCount; ++i)
		{
			// Top and bottom caps are made of single triangles (3 vertices per sector)
			// Middle stacks are made of quads (6 vertices per sector)
			if (i == 0 || i == stackCount - 1)
			{
				indicesPerStack = sectorCount * 3;
			}
			else
			{
				indicesPerStack = sectorCount * 6;
			}
			glUniform4fv(lastColorLoc, 1, myglm::value_ptr(color[i % 6]));

			glDrawElements(GL_TRIANGLES, indicesPerStack, GL_UNSIGNED_INT, (void *)(indexOffset * sizeof(unsigned int)));

			indexOffset += indicesPerStack;
		}
	}
};

class Scene
{
public:
	std::vector<Shape *> shapePointers;

	int selectedShape = 0;
	myglm::mat4 view = myglm::mat4(1.0f);

	// ejcl
	myglm::vec3 cameraPos = myglm::vec3(0.0f, 0.0f, 3.0f);
	myglm::vec3 cameraTarget = myglm::vec3(0.0f, 0.0f, 0.0f);
	myglm::vec3 cameraUp = myglm::vec3(0.0f, 1.0f, 0.0f);

	myglm::mat4 projection;

	unsigned int viewLoc;
	unsigned int projectionLoc;

	unsigned int lastShaderProgram = 0;

	bool isDirty = true;

	bool animationIsRunning = false;

	// myglm::vec3 position(0.0f, 0.0f, 0.0f);
	// myglm::vec3 velocity(dist(gen), dist(gen), 0.0f);

	float lastTime, deltaTime;

	Scene(myglm::vec3 viewPoint = myglm::vec3(0.0f, 0.0f, -3.0f), float fovy = myglm::radians(45.0f), float aspect = (float)WIDTH / (float)HEIGHT, float zNear = 0.1f, float zFar = 100.0f)
	{
		view = myglm::translate(view, viewPoint);
		projection = myglm::perspective(fovy, aspect, zNear, zFar);
	}

	void run(unsigned int shaderProgram)
	{
		updateMatrices(shaderProgram);
		animation();
		for (int i = 0; i < shapePointers.size(); i++)
		{
			shapePointers[i]->draw(shaderProgram);
		}
	}

	void updateTime()
	{
		float currentTime = glfwGetTime();
		deltaTime = currentTime - lastTime;
		lastTime = currentTime;
	}

	void toggleAnimation()
	{
		animationIsRunning = !animationIsRunning;
		lastTime = glfwGetTime();
	}

	void animation()
	{
		if (!animationIsRunning)
		{
			return;
		}
		updateTime();
		// animate
		setShapeVelocity(0, myglm::vec3(0.1f, 0.0f, 0.0f));
	}

	void setShapeVelocity(unsigned int shapeIndex, myglm::vec3 velocity)
	{
		myglm::vec3 delta = velocity * deltaTime;
		shapePointers[shapeIndex]->translate(delta);
	}

	void updateMatrices(unsigned int shaderProgram)
	{
		if (lastShaderProgram != shaderProgram)
		{
			glUseProgram(shaderProgram);
			lastShaderProgram = shaderProgram;

			viewLoc = glGetUniformLocation(shaderProgram, "view");
			projectionLoc = glGetUniformLocation(shaderProgram, "projection");
		}

		if (isDirty)
		{
			glUniformMatrix4fv(viewLoc, 1, GL_FALSE, myglm::value_ptr(view));
			glUniformMatrix4fv(projectionLoc, 1, GL_FALSE, myglm::value_ptr(projection));
			isDirty = false;
		}
	}
	// ejcl movecamera
	float radius = 3.0f;
	float angleY = 0.0f;
	float angleX = 0.0f;

	void updateCamera()
	{
		cameraPos.x = radius * cos(angleX) * sin(angleY);
		cameraPos.y = radius * sin(angleX);
		cameraPos.z = radius * cos(angleX) * cos(angleY);

		myglm::vec3 up(0.0f, 1.0f, 0.0f);

		if (cos(angleX) < 0.0f)
		{
			up = myglm::vec3(0.0f, -1.0f, 0.0f);
		}

		view = myglm::lookAt(
			cameraPos,
			cameraTarget,
			up);

		isDirty = true;
	}
	void rotateCameraY(float amount)
	{
		angleY += amount;
		updateCamera();
	}
	void rotateCameraX(float amount)
	{
		angleX += amount;
		updateCamera();
	}

	std::vector<myglm::vec3> offsetTranslation = {myglm::vec3(0.05f, 0.0f, 0.0f),
												  myglm::vec3(0.0f, 0.05f, 0.0f),
												  myglm::vec3(0.0f, 0.0f, 0.05f)};

	std::vector<myglm::vec3> offsetRotation = {myglm::vec3(0.05f, 0.0f, 0.0f),
											   myglm::vec3(0.0f, 0.05f, 0.0f),
											   myglm::vec3(0.0f, 0.0f, 0.05f)};

	std::vector<myglm::vec3> offsetScale = {myglm::vec3(1.05f, 1.0f, 1.0f),
											myglm::vec3(1.0f, 1.05f, 1.0f),
											myglm::vec3(1.0f, 1.0f, 1.05f)};

	std::vector<myglm::vec3> reset = {myglm::vec3(0.0f, 0.0f, 0.0f),
									  myglm::vec3(0.0f, 0.0f, 0.0f),
									  myglm::vec3(1.0f, 1.0f, 1.0f)};
};

// glUniformMatrix4fv(lastViewLoc, 1, GL_FALSE, myglm::value_ptr(getTranformMatrix(rotateAroundOrigin)));
// glUniformMatrix4fv(lastProjectionLoc, 1, GL_FALSE, myglm::value_ptr(getTranformMatrix(rotateAroundOrigin)));

// callback
void framebuffer_size_callback(GLFWwindow *window, int width, int height)
{
	glViewport(0, 0, width, height);
}

// Inputs
void mouse_button_callback(GLFWwindow *window, int button, int action, int mods)
{

	// PizzaScene* scene = static_cast<PizzaScene*>(glfwGetWindowUserPointer(window));

	// if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS)
	// {
	// double xpos, ypos;
	// glfwGetCursorPos(window, &xpos, &ypos);

	// int width, height;
	// glfwGetWindowSize(window, &width, &height);

	// float x = (2.0f * xpos) / width - 1.0f;
	// float y = 1.0f - (2.0f * ypos) / height;

	// float angle = atan2(y, x);

	// // Normalize angle to 0 to 2*PI
	// if (angle < 0) angle += 2.0f * M_PI;

	// // Map angle to slice index
	// float slice_angle = (2.0f * M_PI) / NUMBER_OF_SLICES;
	// scene->selectedSlice = (int)(angle / slice_angle);

	// std::cout << "Selected Slice: " << scene->selectedSlice << std::endl;
	// }
}

void key_callback(GLFWwindow *window, int key, int scancode, int action, int mods)
{

	Scene *myScene = static_cast<Scene *>(glfwGetWindowUserPointer(window));

	if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
		glfwSetWindowShouldClose(window, true);

	if (action == GLFW_PRESS)
	{
		switch (key)
		{
		case GLFW_KEY_RIGHT:
			myScene->selectedShape++;
			if (myScene->selectedShape == myScene->shapePointers.size())
			{
				myScene->selectedShape = 0;
			}
			break;
		case GLFW_KEY_LEFT:
			myScene->selectedShape--;
			if (myScene->selectedShape == -1)
			{
				myScene->selectedShape = myScene->shapePointers.size() - 1;
			}
			break;
		case GLFW_KEY_X:
			myScene->shapePointers[myScene->selectedShape]->setPosition(myScene->reset[0]);
			myScene->shapePointers[myScene->selectedShape]->setRotation(myScene->reset[1]);
			myScene->shapePointers[myScene->selectedShape]->setScale(myScene->reset[2]);
			break;
		case GLFW_KEY_V:
			myScene->shapePointers[myScene->selectedShape]->swapVisibility();
			break;
		case GLFW_KEY_C:
			myScene->shapePointers[myScene->selectedShape]->displayState();
			break;
		case GLFW_KEY_P:
			myScene->shapePointers[myScene->selectedShape]->swapRotationState();
			break;
		case GLFW_KEY_B:
			myScene->toggleAnimation();
			break;
			// case GLFW_KEY_KP_5:
			// myScene->shapePointers[myScene->selectedShape]->displayState();
			// break;
			// case GLFW_KEY_5:
			// myScene->shapePointers[myScene->selectedShape]->displayState();
			break;
		default:
			break;
		}
	}
	if (action == GLFW_PRESS || action == GLFW_REPEAT)
	{
		switch (key)
		{
			// translate
		case GLFW_KEY_W:
			myScene->shapePointers[myScene->selectedShape]->translate(myScene->offsetTranslation[1]);
			break;
		case GLFW_KEY_S:
			myScene->shapePointers[myScene->selectedShape]->translateInverse(myScene->offsetTranslation[1]);
			break;
		case GLFW_KEY_A:
			myScene->shapePointers[myScene->selectedShape]->translateInverse(myScene->offsetTranslation[0]);
			break;
		case GLFW_KEY_D:
			myScene->shapePointers[myScene->selectedShape]->translate(myScene->offsetTranslation[0]);
			break;
		case GLFW_KEY_Q:
			myScene->shapePointers[myScene->selectedShape]->translateInverse(myScene->offsetTranslation[2]);
			break;
		case GLFW_KEY_E:
			myScene->shapePointers[myScene->selectedShape]->translate(myScene->offsetTranslation[2]);
			break;

			// rotation

		case GLFW_KEY_T:
			myScene->shapePointers[myScene->selectedShape]->rotate(myScene->offsetRotation[0]);
			break;
		case GLFW_KEY_G:
			myScene->shapePointers[myScene->selectedShape]->rotateInverse(myScene->offsetRotation[0]);
			break;
		case GLFW_KEY_F:
			myScene->shapePointers[myScene->selectedShape]->rotate(myScene->offsetRotation[1]);
			break;
		case GLFW_KEY_H:
			myScene->shapePointers[myScene->selectedShape]->rotateInverse(myScene->offsetRotation[1]);
			break;
		case GLFW_KEY_R:
			myScene->shapePointers[myScene->selectedShape]->rotate(myScene->offsetRotation[2]);
			break;
		case GLFW_KEY_Y:
			myScene->shapePointers[myScene->selectedShape]->rotateInverse(myScene->offsetRotation[2]);
			break;
			// Scale

		case GLFW_KEY_I:
			myScene->shapePointers[myScene->selectedShape]->scaleBy(myScene->offsetScale[1]);
			break;
		case GLFW_KEY_K:
			myScene->shapePointers[myScene->selectedShape]->scaleByInverse(myScene->offsetScale[1]);
			break;
		case GLFW_KEY_J:
			myScene->shapePointers[myScene->selectedShape]->scaleByInverse(myScene->offsetScale[0]);
			break;
		case GLFW_KEY_L:
			myScene->shapePointers[myScene->selectedShape]->scaleBy(myScene->offsetScale[0]);
			break;
		case GLFW_KEY_U:
			myScene->shapePointers[myScene->selectedShape]->scaleByInverse(myScene->offsetScale[2]);
			break;
		case GLFW_KEY_O:
			myScene->shapePointers[myScene->selectedShape]->scaleBy(myScene->offsetScale[2]);
			break;

		// global scale
		case GLFW_KEY_N:
			myScene->shapePointers[myScene->selectedShape]->scaleByInverse(myScene->offsetScale[2].z);
			break;
		case GLFW_KEY_M:
			myScene->shapePointers[myScene->selectedShape]->scaleBy(myScene->offsetScale[2].z);
			break;
		case GLFW_KEY_1:
			myScene->rotateCameraX(0.1f);
			break;
		case GLFW_KEY_2:
			myScene->rotateCameraX(-0.1f);
			break;
		case GLFW_KEY_3:
			myScene->rotateCameraY(0.1f);
			break;
		case GLFW_KEY_4:
			myScene->rotateCameraY(-0.1f);
			break;
		default:
			break;
		}
	}
}

int main()
{

	glfwInit();
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	// glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);

	GLFWwindow *window = glfwCreateWindow(WIDTH, HEIGHT, "UwU", NULL, NULL);
	if (window == NULL)
	{
		std::cout << "Failed to create GLFW window" << std::endl;
		glfwTerminate();
		return -1;
	}
	glfwMakeContextCurrent(window);
	glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);

	if (!gladLoadGL(glfwGetProcAddress))
	{
		std::cout << "Failed to initialize GLAD" << std::endl;
		return -1;
	}

	// creamos el vertex
	unsigned int vertexShader;
	vertexShader = glCreateShader(GL_VERTEX_SHADER);
	glShaderSource(vertexShader, 1, &vertexShaderSource, NULL);
	glCompileShader(vertexShader);

	int success;
	char infoLog[512];
	glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);
	if (!success)
	{
		glGetShaderInfoLog(vertexShader, 512, NULL, infoLog);
		std::cout << "ERROR::SHADER::VERTEX::COMPILATION_FAILED\n"
				  << infoLog << std::endl;
	}

	unsigned int fragmentShaders[4];
	const char *fragmentShaderSources[4] = {fragmentShaderSourceUniform,
											fragmentShaderSource1,
											fragmentShaderSource2,
											fragmentShaderSource3};

	for (int i = 0; i < 4; i++)
	{
		fragmentShaders[i] = glCreateShader(GL_FRAGMENT_SHADER);
		glShaderSource(fragmentShaders[i], 1, &fragmentShaderSources[i], NULL);
		glCompileShader(fragmentShaders[i]);
		// error checking
		glGetShaderiv(fragmentShaders[i], GL_COMPILE_STATUS, &success);
		if (!success)
		{
			glGetShaderInfoLog(fragmentShaders[i], 512, NULL, infoLog);
			std::cout << "ERROR::SHADER::FRAGMENT::COMPILATION_FAILED\n"
					  << infoLog << std::endl;
		}
	}

	unsigned int shaderPrograms[4];
	for (int i = 0; i < 4; i++)
	{
		shaderPrograms[i] = glCreateProgram();

		glAttachShader(shaderPrograms[i], vertexShader);
		glAttachShader(shaderPrograms[i], fragmentShaders[i]);
		glLinkProgram(shaderPrograms[i]);

		glGetProgramiv(shaderPrograms[i], GL_LINK_STATUS, &success);
		if (!success)
		{
			glGetProgramInfoLog(shaderPrograms[i], 512, NULL, infoLog);
			std::cout << "ERROR::SHADER::PROGRAM::LINKING_FAILED\n"
					  << infoLog << std::endl;
		}
		glDeleteShader(fragmentShaders[i]);
	}
	glDeleteShader(vertexShader);

	glPointSize(10.0f);
	glLineWidth(5.0f);
	glClearColor(0.2f, 0.3f, 0.3f, 1.0f);

	glEnable(GL_DEPTH_TEST);

	// inputs
	glfwSetMouseButtonCallback(window, mouse_button_callback);
	glfwSetKeyCallback(window, key_callback);

	Scene myScene;
	myScene.updateCamera();

	float sizeRubik = 1.0f;
	float sizeSingleCube = sizeRubik / 3;
	float offSetPos = sizeSingleCube;

	myglm::vec3 scale(sizeSingleCube, sizeSingleCube, sizeSingleCube);
	myglm::vec3 rotation(0.0f, 0.0f, 0.0f);
	myglm::vec3 _position(0.0f, 0.0f, 0.0f);
	std::vector<myglm::vec4> colors = {
		myglm::vec4(1.0f, 1.0f, 1.0f, 1.0f), // White
		myglm::vec4(1.0f, 0.5f, 0.0f, 1.0f), // Orange
		myglm::vec4(0.0f, 0.0f, 1.0f, 1.0f), // Blue
		myglm::vec4(1.0f, 0.0f, 0.0f, 1.0f), // Red
		myglm::vec4(0.0f, 1.0f, 0.0f, 1.0f), // Green
		myglm::vec4(1.0f, 1.0f, 0.0f, 1.0f)	 // Yellow
	};

	myglm::vec4 gray(0.5f, 0.5f, 0.5f, 1.0f);
	std::vector<SingleRubikCube *> cubes;

	for (int i = 0; i < 3; i++)
	{
		for (int j = 0; j < 3; j++)
		{
			for (int k = 0; k < 3; k++)
			{

				myglm::vec3 position(
					_position.x + sizeSingleCube * k - offSetPos,
					_position.y + sizeSingleCube * j - offSetPos,
					_position.z + sizeSingleCube * i - offSetPos);

				std::vector<myglm::vec4> fColors(6, gray);

				if (i == 0)
					fColors[0] = myglm::vec4(colors[4]); // Front face (-Z) gets Green
				if (i == 2)
					fColors[1] = myglm::vec4(colors[2]); // Back face (+Z) gets Blue
				if (k == 0)
					fColors[2] = myglm::vec4(colors[1]); // Left face (-X) gets Orange
				if (k == 2)
					fColors[3] = myglm::vec4(colors[3]); // Right face (+X) gets Red
				if (j == 2)
					fColors[4] = myglm::vec4(colors[0]); // Up face (+Y) gets White
				if (j == 0)
					fColors[5] = myglm::vec4(colors[5]); // Down face (-Y) gets Yellow

				cubes.push_back(new SingleRubikCube(position, scale, rotation, fColors));
				myScene.shapePointers.push_back(cubes[cubes.size() - 1]);
			}
		}
	}

	// myglm::vec3 position1(0.4f, 0.0f, 0.0f);
	// myglm::vec3 scale1(0.5f, 0.5f, 0.5f);
	// myglm::vec3 rotation1(0.0f, 0.0f, 0.0f);
	// Cube cube1(position1, scale1, rotation1);
	// myScene.shapePointers.push_back(&cube1);

	// myglm::vec3 position2(-0.4f, 0.0f, 0.0f);
	// myglm::vec3 scale2(0.5f, 0.5f, 0.5f);
	// myglm::vec3 rotation2(0.5f, 0.5f, 0.5f);
	// Cube cube2(position2, scale2, rotation2);
	// myScene.shapePointers.push_back(&cube2);

	// myglm::vec3 position3(-0.4f, 0.0f, 0.0f);
	// myglm::vec3 scale3(0.5f, 0.5f, 0.5f);
	// myglm::vec3 rotation3(0.0f, 0.0f, 0.0f);
	// Pyramid pyra1(position3, scale3, rotation3, getRadiusForNEdges(4),1,4);
	// myScene.shapePointers.push_back(&pyra1);

	// std::cout<<getRadiusForNEdges(3)<<std::endl;

	// myglm::vec3 position4(0.0f, 0.5f, 0.0f);
	// myglm::vec3 scale4(0.5f, 0.5f, 0.5f);
	// myglm::vec3 rotation4(0.0f, 0.0f, 0.0f);
	// Sphere sphere1(position4, scale4, rotation4, 0.5f, 50, 25);
	// myScene.shapePointers.push_back(&sphere1);

	// myglm::vec3 position5(0.0f, -0.5f, 0.0f);
	// myglm::vec3 scale5(0.5f, 0.5f, 0.5f);
	// myglm::vec3 rotation5(0.5f, 0.5f, 0.5f);
	// Prism prims1(position5, scale5, rotation5);
	// myScene.shapePointers.push_back(&prims1);

	// myglm::vec3 position6(-0.5f, -0.5f, 0.0f);
	// myglm::vec3 scale6(0.5f, 0.5f, 0.5f);
	// myglm::vec3 rotation6(0.5f, 0.5f, 0.5f);
	// Prism prims2(position6, scale6, rotation6,0.25f,0.5f,4);
	// myScene.shapePointers.push_back(&prims2);

	// myglm::vec3 position7(0.5f, -0.5f, 0.0f);
	// myglm::vec3 scale7(0.5f, 0.5f, 0.5f);
	// myglm::vec3 rotation7(0.5f, 0.5f, 0.5f);
	// Prism prims3(position7, scale7, rotation7,0.5f,0.5f,100);
	// myScene.shapePointers.push_back(&prims3);

	glfwSetWindowUserPointer(window, &myScene);

	std::cout << "**LEFT-RIGHT to select a shape\n\n"
			  <<

		"A-D to translate in the x-axis\n"
			  << "W-S to translate in the y-axis\n"
			  << "Q-E to translate in the z-axis\n\n"
			  <<

		"T-G to rotate around x-axis\n"
			  << "F-H to rotate around y-axis\n"
			  << "R-Y to rotate around z-axis\n\n"
			  <<

		"J-L to scale in the x-axis\n"
			  << "I-K to scale in the y-axis\n"
			  << "U-O to scale in the z-axis\n\n"
			  <<

		"N-M to scale in all axes\n\n"
			  <<

		"X to set to default state\n"
			  << "C to set to default state\n"
			  << "V to swap visibility\n"
			  << "ESC to close window\n\n";

	while (!glfwWindowShouldClose(window))
	{
		// glClear(GL_COLOR_BUFFER_BIT);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		myScene.run(shaderPrograms[0]);

		glfwSwapBuffers(window);
		glfwPollEvents();
	}

	for (int i = 0; i < 4; i++)
	{
		glDeleteProgram(shaderPrograms[i]);
	}

	glfwTerminate();
	return 0;
}