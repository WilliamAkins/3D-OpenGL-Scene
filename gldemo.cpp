#include "stdafx.h"
#include <glew\glew.h>
#include <freeglut\freeglut.h>
#include <CoreStructures\CoreStructures.h>
#include "texture_loader.h"
#include "shader_setup.h"
#include "CGModel\CGModel.h"
#include "Importers\CGImporters.h"
#include <stack>

using namespace std;
using namespace CoreStructures;

#define FIXED_FUNCTION false

// Globals
//Define pi for use with angles
static const float PI = 3.14159;

const int NUM_OF_LIGHTS = 3;
const int NUM_OF_MATS = 4;

// Mouse input (rotation) example
// Variable we'll use to animate (rotate) our star object
float theta = 0.0f;
float cameraX = 0.0f;

GUClock cgClock;
// Variables needed to track where the mouse pointer is so we can determine which direction it's moving in
int mouse_x, mouse_y;
bool mDown = false;

struct rocketAttrib {
	float x = 0.0f;
	float y = 0.0f;
	float rotX = 0.0f;
	float rotY = 90 * (PI/180);
	bool launchStarted = false;
	bool movingUp = true;
	bool isRotating = false;
	bool hasExploded = false;
} rocketAttrib;

struct flameAttrib {
	float x = 0.0f;
	float y = 0.0f;
	float scale = 1.0f;
	float offsetY = 0.0f;
	bool isGrowing = true;
} flameAttrib;

struct explosionAttrib {
	float x = 0.0f;
	float y = 10.0f;
	float offsetY = 0.0f;
	float scale = 0.0f;
	bool isGrowing = true;
	bool hasFinished = false;
} explosionAttrib;

// uniform variables to access in shader
GLuint gDiffuse[NUM_OF_LIGHTS]; // Shader light diffuse
GLuint gAmbient[NUM_OF_LIGHTS]; // Shader light ambient
GLuint gAttenuation[NUM_OF_LIGHTS]; // Shader light attenuation
GLuint gLPosition[NUM_OF_LIGHTS]; // Shader light position

GLuint gEyePos; // Shader eye position

//Material Properties
GLuint gMatAmbient[NUM_OF_MATS];
GLuint gMatDiffuse[NUM_OF_MATS];
GLuint gMatSpecular[NUM_OF_MATS];

// Shader Matrix setup
GLuint gModelMatrix;
GLuint gViewMatrix;
GLuint gProjectionMatrix;
GLuint gNormalMatrix;

GLuint tangent;

// Shader program object
GLuint myShaderProgram;

// Textures
GLuint rocket_texture = 0;
GLuint rocket_normal = 2;

GLuint flame_texture = 0;
GLuint flame_normal = 2;

GLuint terrain_texture = 0;
GLuint terrain_normal = 2;

GLuint explosion_texture = 0;
GLuint explosion_normal = 2;

// Model
CGModel *rocket = NULL;
CGModel *flame = NULL;
CGModel *terrain = NULL;
CGModel *explosion = NULL;

// Model Instance Matrix
GUMatrix4 rocketMatrix;
GUMatrix4 flameMatrix;
GUMatrix4 terrainMatrix;
GUMatrix4 explosionMatrix;

GUPivotCamera *mainCamera = NULL;

std::stack<GUMatrix4> matrixStack;

struct Lights {
	GUVector4 light_ambient;
	GUVector4 light_diffuse;
	GUVector4 light_specular;
	GUVector4 light_position;
} dirLight, flameLight, rocketLight;

enum Objects {enumRocket, enumFlame, enumTerrain, enumExplosion};

struct Materials {
	GUVector4 mat_amb_diff;
	GUVector4 mat_specular;
	GUVector4 mat_emission;
	GLfloat mat_shininess;
} rocketMat, flameMat, terrainMat, explosionMat;

// Function Prototypes
void init(int argc, char* argv[]);
void setupRocket(void);
void setupFlame(void);
void setupTerrain(void);
void setupExplosion(void);
void report_version(void);
void update(void);
void display(void);
void displayRocket(void);
void displayFlame(void);
void displayTerrain(GUMatrix4*);
void displayExplosion(GUMatrix4*);
void mouseButtonDown(int button_id, int state, int x, int y);
void mouseMove(int x, int y);
void keyDown(unsigned char key, int x, int y);
void mouseWheelUpdate(int wheel_number, int direction, int x, int y);


int _tmain(int argc, char* argv[]) {
	init(argc, argv);

	glutMainLoop();

	//Shut down COM
	shutdownCOM();

	return 0;
}


void init(int argc, char* argv[]) {

	// Initialise COM so we can use Windows Imaging Component
	initCOM();

	// Initialise FreeGLUT
	glutInit(&argc, argv);

	glutInitContextVersion(3, 3);
	glutInitContextProfile(GLUT_COMPATIBILITY_PROFILE);
	glutInitDisplayMode(GLUT_RGBA | GLUT_DEPTH | GLUT_DOUBLE);

	glutInitWindowSize(800, 800);
	glutInitWindowPosition(64, 64);
	glutCreateWindow("William Akins - 15029476 | CS2S565");

	// Register callback functions
	glutDisplayFunc(display);
	glutKeyboardFunc(keyDown);
	glutMouseFunc(mouseButtonDown);
	glutMotionFunc(mouseMove);
	glutMouseWheelFunc(mouseWheelUpdate);
	// Initialise GLEW library
	GLenum err = glewInit();

	// Ensure GLEW was initialised successfully before proceeding
	if (err == GLEW_OK) {

		cout << "GLEW initialised okay\n";

	} else {

		cout << "GLEW could not be initialised\n";
		throw;
	}
	
	// Example query OpenGL state (get version number)
	report_version();

	// Initialise OpenGL...

	// Setup colour to clear the window
	glClearColor(0.2431, 0.5451, 0.8, 0.0f);

	// setup main 3D camera
	mainCamera = new GUPivotCamera(0.0f, 0.0f, 55.0f, 55.0f, 1.0f, 0.1f);

	glEnable(GL_CULL_FACE);
	glFrontFace(GL_CCW);

	glDepthFunc(GL_LEQUAL);
	glEnable(GL_DEPTH_TEST);

	glLineWidth(4.0f);

	//setup the vertex and fragment shaders
	myShaderProgram = setupShaders(string("Shaders\\diffuse_vertex_shader.txt"), string("Shaders\\diffuse_fragment_shader.txt"));

	for (int i = 0; i < NUM_OF_LIGHTS; i++) {
		//char array holds the name of each shader variable using c style sprintf
		char shaderVars[4][16];
		sprintf_s(shaderVars[0], "lposition[%i]", i);
		sprintf_s(shaderVars[1], "lambient[%i]", i);
		sprintf_s(shaderVars[2], "ldiffuse[%i]", i);
		sprintf_s(shaderVars[3], "lattenuation[%i]", i);

		gLPosition[i] = glGetUniformLocation(myShaderProgram, shaderVars[0]);
		gAmbient[i] = glGetUniformLocation(myShaderProgram, shaderVars[1]);
		gDiffuse[i] = glGetUniformLocation(myShaderProgram, shaderVars[2]);
		gAttenuation[i] = glGetUniformLocation(myShaderProgram, shaderVars[3]);
	}

	for (int i = 0; i < 1; i++) {
		gMatAmbient[i] = glGetUniformLocation(myShaderProgram, "matAmbient");
		gMatDiffuse[i] = glGetUniformLocation(myShaderProgram, "matDiffuse");
		gMatSpecular[i] = glGetUniformLocation(myShaderProgram, "matSpecular");
	}

	// get matrix unifom locations in shader
	gModelMatrix = glGetUniformLocation(myShaderProgram, "g_ModelMatrix");
	gViewMatrix = glGetUniformLocation(myShaderProgram, "g_ViewMatrix");
	gProjectionMatrix = glGetUniformLocation(myShaderProgram, "g_ProjectionMatrix");
	gNormalMatrix = glGetUniformLocation(myShaderProgram, "g_NormalMatrix");

	// Enable Vertex Arrays
	// Tell OpenGL to expect vertex position information from an array
	glEnableClientState(GL_VERTEX_ARRAY);
	// Tell OpenGL to expect vertex colour information from an array
	glEnableClientState(GL_COLOR_ARRAY);
	// Tell OpenGL to expect vertex colour information from an array
	glEnableClientState(GL_NORMAL_ARRAY);
	// Tell OpenGL to expect texture coordinate information from an array
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);

	setupRocket();

	setupFlame();

	setupTerrain();

	setupExplosion();

	//directional lighting properties
	dirLight.light_ambient = { 0.3f, 0.3f, 0.3f, 1.0f }; //tint
	dirLight.light_diffuse = { 0.5f, 0.5f, 0.5f, 0.5f }; //tint
	dirLight.light_specular = { 1.0f, 1.0f, 1.0f, 1.0f }; // White highlight
	dirLight.light_position = { 4.0f, 4.0f, 4.0f, 4.0f }; // Point light (w=1.0)

	flameLight.light_ambient = { 0.001f, 0.001f, 0.001f, 0.001f }; //tint
	flameLight.light_diffuse = { 0.855f, 0.03f, 0.03f, 10.0f }; //tint
	flameLight.light_specular = { 0.855f, 0.03f, 0.03f, 20.0f }; // White highlight
	flameLight.light_position = { 0.0f, -11.0f, 0.0f, 1.0f }; // Point light (w=1.0)

	rocketLight.light_ambient = { 0.0f, 0.0f, 0.0f, 0.0f }; //tint
	rocketLight.light_diffuse = { 1.0f, 0.871f, 0.0f, 3.0f }; //tint
	rocketLight.light_specular = { 1.0f, 0.871f, 0.0f, 8.0f }; // White highlight
	rocketLight.light_position = { 0.0f, 7.0f, 0.0f, 1.0f }; // Point light (w=1.0)

	cgClock.start();
}

void setupRocket() {
	rocket_texture = fiLoadTexture("Resources\\Textures\\rocket_texture.png");
	//rocket_normal = fiLoadTexture("Resources\\Textures\\rocket_normal.png");

	// load rocket model
	rocket = new CGModel();
	importOBJ(L"Resources\\Models\\rocket.obj", rocket);
	//rocket->setNormalMapForModel(rocket_normal);
	rocket->setTextureForModel(rocket_texture);
	rocketMatrix = GUMatrix4::identity();

	//rocket materials
	rocketMat.mat_amb_diff = { 1.5f, 1.5f, 1.5f, 1.0f }; //Texture will provide ambient and diffuse.
	rocketMat.mat_specular = { 0.8f, 0.8f, 0.8f, 1.0f }; //White highlight
	rocketMat.mat_emission = { 0.0f, 0.0f, 0.0f, 0.0f }; //Not emissive
	rocketMat.mat_shininess = 350.0f; // Shiny surface

	//rocket->rotate(GUQuaternion(0.0f, 90 * (PI / 180), 0.0f));
}

void setupFlame() {
	flame_texture = fiLoadTexture("Resources\\Textures\\flame_texture.png");
	//flame_normal = fiLoadTexture("Resources\\Textures\\flame_normal.png");

	// load flame model
	flame = new CGModel();
	importOBJ(L"Resources\\Models\\flame.obj", flame);
	//flame->setNormalMapForModel(flame_normal);
	flame->setTextureForModel(flame_texture);
	flameMatrix = GUMatrix4::identity();

	//flame materials
	flameMat.mat_amb_diff = { 0.0f, 0.0f, 0.0f, 0.0f }; //Texture will provide ambient and diffuse.
	flameMat.mat_specular = { 0.0f, 0.0f, 0.0f, 0.0f }; //White highlight
	flameMat.mat_emission = { 1.0f, 1.0f, 1.0f, 1.0f }; //Not emissive
	flameMat.mat_shininess = 0.0f; // Shiny surface
}

void setupTerrain() {
	terrain_texture = fiLoadTexture("Resources\\Textures\\floor_texture.png");

	// load flame model
	terrain = new CGModel();
	importOBJ(L"Resources\\Models\\floor.obj", terrain);
	//terrain->setNormalMapForModel(terrain_normal);
	terrain->setTextureForModel(terrain_texture);
	terrainMatrix = GUMatrix4::identity();

	//terrain materials
	terrainMat.mat_amb_diff = { 0.0f, 0.0f, 0.0f, 0.0f }; //Texture will provide ambient and diffuse.
	terrainMat.mat_specular = { 0.0f, 0.0f, 0.0f, 0.0f }; //White highlight
	terrainMat.mat_emission = { 0.0f, 0.0f, 0.0f, 0.0f }; //Not emissive
	terrainMat.mat_shininess = 0.0f; // Shiny surface

	terrain->translate(GUVector4(0.0f, -6.5f, 0.0f));
}

void setupExplosion() {
	explosion_texture = fiLoadTexture("Resources\\Textures\\explosion_texture.png");

	// load flame model
	explosion = new CGModel();
	importOBJ(L"Resources\\Models\\explosion.obj", explosion);
	//explosion->setNormalMapForModel(explosion_normal);
	explosion->setTextureForModel(explosion_texture);
	explosionMatrix = GUMatrix4::identity();

	//terrain materials
	explosionMat.mat_amb_diff = { 0.0f, 0.0f, 0.0f, 0.0f }; //Texture will provide ambient and diffuse.
	explosionMat.mat_specular = { 0.0f, 0.0f, 0.0f, 0.0f }; //White highlight
	explosionMat.mat_emission = { 0.0f, 0.0f, 0.0f, 0.0f }; //Not emissive
	explosionMat.mat_shininess = 0.0f; // Shiny surface

	explosion->translate(GUVector4(0.0f, -6.5f, 0.0f));
}

void report_version(void) {
	int majorVersion, minorVersion;

	glGetIntegerv(GL_MAJOR_VERSION, &majorVersion);
	glGetIntegerv(GL_MINOR_VERSION, &minorVersion);

	cout << "OpenGL version " << majorVersion << "." << minorVersion << "\n\n";
}

// Update Animated Scene Objects Using Delta Time
void update(void) {
	cgClock.tick();
	theta += cgClock.gameTimeDelta();

	if (rocketAttrib.launchStarted && !rocketAttrib.hasExploded) {
		if (rocketAttrib.movingUp)
			rocketAttrib.y += 10.0f  * cgClock.gameTimeDelta();
		//only true if the rocket has finished rotating
		else if (rocketAttrib.isRotating == false)
			rocketAttrib.y -= 10.0f * cgClock.gameTimeDelta();

		//move the rocket upwards until it reaches its max height
		if (rocketAttrib.y >= 50.0f && rocketAttrib.movingUp == true) {
			rocketAttrib.movingUp = false;
			rocketAttrib.isRotating = true;
		}

		if (rocketAttrib.isRotating) {
			//rotate the rocket on the x axis over time
			rocketAttrib.rotX += 0.5 * (PI / 180);
			//if the rocket is facing down, then stop rotating
			if (rocketAttrib.rotX >= (180 * (PI / 180)))
				rocketAttrib.isRotating = false;
		}

		if (rocketAttrib.y <= -3.0f) {
			rocketAttrib.hasExploded = true;

			//change the ambient lighting in the scene to red
			dirLight.light_ambient = { 0.855f, 0.03f, 0.03f, 0.5f };
		}
	} else if (rocketAttrib.hasExploded && !explosionAttrib.hasFinished) {
		//expands and contracts the explosion object once the rocket has exploded
		switch (explosionAttrib.isGrowing) {
			case true:
				explosionAttrib.scale += 0.1f;
				explosionAttrib.offsetY += 0.5f;
				break;
			case false:
				explosionAttrib.scale -= 0.1f;
				explosionAttrib.offsetY -= 0.5f;
				break;
			default:
				break;
		}

		//begins the shrinking process of the explosion
		if (explosionAttrib.scale >= 8.0f)
			explosionAttrib.isGrowing = false;
		else if (explosionAttrib.scale <= 0.0f) {
			explosionAttrib.hasFinished = true;
			//reset the ambient scene light to normal
			dirLight.light_ambient = { 0.3f, 0.3f, 0.3f, 1.0f };
		}
	}


	//changes the flame position and scale over time
	switch (flameAttrib.isGrowing) {
		case true:
			flameAttrib.scale += 0.002f;
			flameAttrib.offsetY -= 0.01f;
			break;
		case false:
			flameAttrib.scale -= 0.002f;
			flameAttrib.offsetY += 0.01f;
			break;
		default:
			break;
	}

	//if the flame reaches it's max scale, then shrink
	if (flameAttrib.scale >= 1.4f)
		flameAttrib.isGrowing = false;
	else if (flameAttrib.scale <= 0.8f)
		flameAttrib.isGrowing = true;
}

// Example rendering functions
void display(void) {
	// Update Animated Scene Objects Using Delta Time
	update();

	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	// get camera view and projection transforms
	GUMatrix4 viewMatrix = mainCamera->viewTransform();
	GUMatrix4 projMatrix = mainCamera->projectionTransform();
 
	// Get Camera Position in World Coordinates
	GUVector4 eyePos = mainCamera->cameraLocation();

	// Render using finished shader
	glUseProgram(myShaderProgram);
	GLfloat	attenuation[] = { 0.5, 0.5, 0.04 };

	glUniform4fv(gDiffuse[0], 1, (GLfloat*)&dirLight.light_diffuse);
	glUniform4fv(gAmbient[0], 1, (GLfloat*)&dirLight.light_ambient);
	glUniform4fv(gLPosition[0], 1, (GLfloat*)&dirLight.light_position);
	glUniform3fv(gAttenuation[0], 1, (GLfloat*)&attenuation);

	glUniform4fv(gDiffuse[1], 1, (GLfloat*)&flameLight.light_diffuse);
	glUniform4fv(gAmbient[1], 1, (GLfloat*)&flameLight.light_ambient);
	glUniform4fv(gLPosition[1], 1, (GLfloat*)&flameLight.light_position);
	glUniform3fv(gAttenuation[1], 1, (GLfloat*)&attenuation);
	flameLight.light_position = { 0.0f, rocketAttrib.y - 11.0f, 0.0f, 1.0f };

	glUniform4fv(gDiffuse[2], 1, (GLfloat*)&rocketLight.light_diffuse);
	glUniform4fv(gAmbient[2], 1, (GLfloat*)&rocketLight.light_ambient);
	glUniform4fv(gLPosition[2], 1, (GLfloat*)&rocketLight.light_position);
	glUniform3fv(gAttenuation[2], 1, (GLfloat*)&attenuation);
	rocketLight.light_position = { 0.0f, rocketAttrib.y + 7.0f, 0.0f, 1.0f };

	glUniform4fv(gEyePos, 1, (GLfloat*)&eyePos);

	GUMatrix4 normalMatrix;

	//setup matrix as an identity matrix
	GUMatrix4 M = GUMatrix4::identity();

	matrixStack.push(M);

	//Transform the base object, the missile body
	GUMatrix4 T = GUMatrix4::translationMatrix(rocketAttrib.x, rocketAttrib.y, 0.0f);
	GUMatrix4 R = GUMatrix4::rotationMatrix(rocketAttrib.rotX, rocketAttrib.rotY, 0.0f);
	M = T * R;

	// Get Normal Matrix (transform Normals to World Coordinates)
	normalMatrix = rocketMatrix.inverseTranspose();

	//generate model matrix for the rocket
	glUniformMatrix4fv(gModelMatrix, 1, false, (GLfloat*)&rocketMatrix);

	glUniformMatrix4fv(gModelMatrix, 1, GL_FALSE, (GLfloat*)&M);

	//display the various objects in the world
	//only display the rocket object if the rocket explosion hasn't fully grown
	if (explosionAttrib.isGrowing)
		displayRocket();


	matrixStack.push(M);
	M = M * GUMatrix4::translationMatrix(0.0f, -11.3f + flameAttrib.offsetY, 0.0f)  * GUMatrix4::scaleMatrix(1.0f, flameAttrib.scale, 1.0f);

	// Get Normal Matrix (transform Normals to World Coordinates)
	normalMatrix = flameMatrix.inverseTranspose();

	//generate model matrix for the flame
	glUniformMatrix4fv(gModelMatrix, 1, false, (GLfloat*)&flameMatrix);

	glUniformMatrix4fv(gModelMatrix, 1, GL_FALSE, (GLfloat*)&M);

	//only display the flame object if the rocket explosion hasn't fully grown
	if (explosionAttrib.isGrowing && rocketAttrib.launchStarted)
		displayFlame();

	M = matrixStack.top();
	matrixStack.pop();

	M = matrixStack.top();
	matrixStack.pop();


	displayTerrain(&normalMatrix);

	//only show the explosion if it hasn't already been expanded and used
	if (!explosionAttrib.hasFinished)
		displayExplosion(&normalMatrix);

	//generate the various model view matrices
	glUniformMatrix4fv(gViewMatrix, 1, false, (GLfloat*)&viewMatrix);
	glUniformMatrix4fv(gProjectionMatrix, 1, false, (GLfloat*)&projMatrix);
	glUniformMatrix4fv(gNormalMatrix, 1, false, (GLfloat*)&normalMatrix);

	glUseProgram(0);

	glutSwapBuffers();

	//Redraw is required for animation
	glutPostRedisplay();
}

void displayRocket() {
	if (rocket) {
		// Material Properties
		glUniform4fv(gMatAmbient[enumRocket], 1, (GLfloat*)&rocketMat.mat_amb_diff);
		glUniform4fv(gMatDiffuse[enumRocket], 1, (GLfloat*)&rocketMat.mat_amb_diff);
		glUniform4fv(gMatSpecular[enumRocket], 1, (GLfloat*)&rocketMat.mat_amb_diff);

		// The shader requires shininess in w parameter hence:
		GLfloat mat_specular_shader[] = { rocketMat.mat_specular.x, rocketMat.mat_specular.y, rocketMat.mat_specular.z, rocketMat.mat_shininess }; // White highlight
		glUniform4fv(gMatSpecular[enumRocket], 1, (GLfloat*)&mat_specular_shader);

		//rocket->renderNormalMapped(rocket_normal);
		rocket->renderTexturedModel();
	}
}

void displayFlame() {
	//glUseProgram(myShaderProgram);

	if (flame) {
		// Material Properties
		glUniform4fv(gMatAmbient[enumFlame], 1, (GLfloat*)&flameMat.mat_amb_diff);
		glUniform4fv(gMatDiffuse[enumFlame], 1, (GLfloat*)&flameMat.mat_amb_diff);
		glUniform4fv(gMatSpecular[enumFlame], 1, (GLfloat*)&flameMat.mat_amb_diff);

		//flame->renderNormalMapped(flame_normal);
		flame->renderTexturedModel();
	}

	//glUseProgram(0);
}

void displayTerrain(GUMatrix4 *normalMatrix) {
	if (terrain) {
		// Get Normal Matrix (transform Normals to World Coordinates)
		*normalMatrix = terrainMatrix.inverseTranspose();

		//generate multiple model matrices
		glUniformMatrix4fv(gModelMatrix, 1, false, (GLfloat*)&terrainMatrix);

		// Material Properties
		glUniform4fv(gMatAmbient[enumTerrain], 1, (GLfloat*)&terrainMat.mat_amb_diff);
		glUniform4fv(gMatDiffuse[enumTerrain], 1, (GLfloat*)&terrainMat.mat_amb_diff);
		glUniform4fv(gMatSpecular[enumTerrain], 1, (GLfloat*)&terrainMat.mat_amb_diff);

		//terrain->renderNormalMapped(terrain_normal);
		terrain->renderTexturedModel();
	}
}

void displayExplosion(GUMatrix4 *normalMatrix) {
	if (explosion) {
		// Get Normal Matrix (transform Normals to World Coordinates)
		*normalMatrix = explosionMatrix.inverseTranspose();

		//generate multiple model matrices
		glUniformMatrix4fv(gModelMatrix, 1, false, (GLfloat*)&explosionMatrix);

		// Material Properties
		glUniform4fv(gMatAmbient[enumExplosion], 1, (GLfloat*)&explosionMat.mat_amb_diff);
		glUniform4fv(gMatDiffuse[enumExplosion], 1, (GLfloat*)&explosionMat.mat_amb_diff);
		glUniform4fv(gMatSpecular[enumExplosion], 1, (GLfloat*)&explosionMat.mat_amb_diff);

		//terrain->renderNormalMapped(terrain_normal);
		explosion->renderTexturedModel();

		explosionMatrix = GUMatrix4::translationMatrix(explosionAttrib.x, explosionAttrib.y + explosionAttrib.offsetY, 0.0f) *
						  GUMatrix4::scaleMatrix(explosionAttrib.scale, explosionAttrib.scale, explosionAttrib.scale);
	}
}

#pragma region Event handling functions
void mouseButtonDown(int button_id, int state, int x, int y) {
	if (button_id == GLUT_LEFT_BUTTON) {
		if (state == GLUT_DOWN) {

			mouse_x = x;
			mouse_y = y;

			mDown = true;

		} else if (state == GLUT_UP) {

			mDown = false;
		}
	}
}


void mouseMove(int x, int y) {
	if (mDown) {

		int dx = x - mouse_x;
		int dy = y - mouse_y;

		//theta += (float)dy * (3.142f * 0.01f);

		// rotate camera around the origin
		if (mainCamera)
			mainCamera->transformCamera((float)-dy, (float)-dx, 0.0f);

		mouse_x = x;
		mouse_y = y;

		glutPostRedisplay();
	}
}


void mouseWheelUpdate(int wheel_number, int direction, int x, int y) {
	if (mainCamera) {

		if (direction<0)
			mainCamera->scaleCameraRadius(1.1f);
		else if (direction>0)
			mainCamera->scaleCameraRadius(0.9f);

		glutPostRedisplay();
	}
}


void keyDown(unsigned char key, int x, int y) {
	//if (key == 'r') {
	//	theta = 0.0f;
	//	glutPostRedisplay();
	//}

	//if (key == 'a') {
		//cameraX += 2.2f;
		//mainCamera->transformCamera(0.0f, cameraX, 0.0f);
		//mainCamera->
	//}

	//begin the launch of the rocket
	if (key == ' ')
		rocketAttrib.launchStarted = true;
}
#pragma endregion