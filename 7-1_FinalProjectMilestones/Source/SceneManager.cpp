///////////////////////////////////////////////////////////////////////////////
// scenemanager.cpp
// ============
// manage the preparing and rendering of 3D scenes - textures, materials, lighting
//
//  AUTHOR: Brian Battersby - SNHU Instructor / Computer Science
//	Created for CS-330-Computational Graphics and Visualization, Nov. 1st, 2023
///////////////////////////////////////////////////////////////////////////////

#include "SceneManager.h"

#ifndef STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#endif

#include <glm/gtx/transform.hpp>

// declaration of global variables
namespace
{
	const char* g_ModelName = "model";
	const char* g_ColorValueName = "objectColor";
	const char* g_TextureValueName = "objectTexture";
	const char* g_UseTextureName = "bUseTexture";
	const char* g_UseLightingName = "bUseLighting";
}

/***********************************************************
 *  SceneManager()
 ***********************************************************/
SceneManager::SceneManager(ShaderManager* pShaderManager)
{
	m_pShaderManager = pShaderManager;
	m_basicMeshes = new ShapeMeshes();

	// init texture tracking 
	m_loadedTextures = 0;
	m_objectMaterials.clear();
}

/***********************************************************
 *  ~SceneManager()
 ***********************************************************/
SceneManager::~SceneManager()
{
	m_pShaderManager = NULL;
	delete m_basicMeshes;
	m_basicMeshes = NULL;
}

/***********************************************************
 *  CreateGLTexture()
 ***********************************************************/
bool SceneManager::CreateGLTexture(const char* filename, std::string tag)
{
	int width = 0;
	int height = 0;
	int colorChannels = 0;
	GLuint textureID = 0;

	stbi_set_flip_vertically_on_load(true);

	unsigned char* image = stbi_load(filename, &width, &height, &colorChannels, 0);

	if (!image)
	{
		std::cout << "Could not load image:" << filename << std::endl;
		return false;
	}

	std::cout << "Successfully loaded image:" << filename
		<< ", width:" << width
		<< ", height:" << height
		<< ", channels:" << colorChannels << std::endl;

	glGenTextures(1, &textureID);
	glBindTexture(GL_TEXTURE_2D, textureID);

	// WRAP (must be set while bound)
	if (tag == "keyboard" || tag == "mouse")
	{
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	}
	else
	{
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	}

	// FILTER
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	// (optional) aniso
	float aniso = 0.0f;
	glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY, &aniso);
	if (aniso < 1.0f) aniso = 1.0f;
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY, aniso);

	// Upload image (while still bound)
	if (colorChannels == 3)
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, image);
	else if (colorChannels == 4)
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, image);
	else
		return false;

	glGenerateMipmap(GL_TEXTURE_2D);

	stbi_image_free(image);
	glBindTexture(GL_TEXTURE_2D, 0);

	// register + return true
	m_textureIDs[m_loadedTextures].ID = textureID;
	m_textureIDs[m_loadedTextures].tag = tag;
	m_loadedTextures++;
	return true;
}

/***********************************************************
 *  BindGLTextures()
 ***********************************************************/
void SceneManager::BindGLTextures()
{
	for (int i = 0; i < m_loadedTextures; i++)
	{
		glActiveTexture(GL_TEXTURE0 + i);
		glBindTexture(GL_TEXTURE_2D, m_textureIDs[i].ID);
	}
}

/***********************************************************
 *  DestroyGLTextures()
 ***********************************************************/
void SceneManager::DestroyGLTextures()
{
	for (int i = 0; i < m_loadedTextures; i++)
	{
		glDeleteTextures(1, &m_textureIDs[i].ID);
	}
}

/***********************************************************
 *  FindTextureID()
 ***********************************************************/
int SceneManager::FindTextureID(std::string tag)
{
	int textureID = -1;
	int index = 0;
	bool bFound = false;

	while ((index < m_loadedTextures) && (bFound == false))
	{
		if (m_textureIDs[index].tag.compare(tag) == 0)
		{
			textureID = m_textureIDs[index].ID;
			bFound = true;
		}
		else
			index++;
	}

	return(textureID);
}

/***********************************************************
 *  FindTextureSlot()
 ***********************************************************/
int SceneManager::FindTextureSlot(std::string tag)
{
	int textureSlot = -1;
	int index = 0;
	bool bFound = false;

	while ((index < m_loadedTextures) && (bFound == false))
	{
		if (m_textureIDs[index].tag.compare(tag) == 0)
		{
			textureSlot = index;
			bFound = true;
		}
		else
			index++;
	}

	return(textureSlot);
}

/***********************************************************
 *  FindMaterial()
 ***********************************************************/
bool SceneManager::FindMaterial(std::string tag, OBJECT_MATERIAL& material)
{
	if (m_objectMaterials.size() == 0)
		return false;

	int index = 0;
	bool bFound = false;

	while ((index < (int)m_objectMaterials.size()) && (bFound == false))
	{
		if (m_objectMaterials[index].tag.compare(tag) == 0)
		{
			bFound = true;
			material.diffuseColor = m_objectMaterials[index].diffuseColor;
			material.specularColor = m_objectMaterials[index].specularColor;
			material.shininess = m_objectMaterials[index].shininess;
		}
		else
		{
			index++;
		}
	}

	return bFound;
}

/***********************************************************
 *  SetTransformations()
 ***********************************************************/
void SceneManager::SetTransformations(
	glm::vec3 scaleXYZ,
	float XrotationDegrees,
	float YrotationDegrees,
	float ZrotationDegrees,
	glm::vec3 positionXYZ)
{
	glm::mat4 modelView;
	glm::mat4 scale;
	glm::mat4 rotationX;
	glm::mat4 rotationY;
	glm::mat4 rotationZ;
	glm::mat4 translation;

	scale = glm::scale(scaleXYZ);
	rotationX = glm::rotate(glm::radians(XrotationDegrees), glm::vec3(1.0f, 0.0f, 0.0f));
	rotationY = glm::rotate(glm::radians(YrotationDegrees), glm::vec3(0.0f, 1.0f, 0.0f));
	rotationZ = glm::rotate(glm::radians(ZrotationDegrees), glm::vec3(0.0f, 0.0f, 1.0f));
	translation = glm::translate(positionXYZ);

	modelView = translation * rotationZ * rotationY * rotationX * scale;

	if (NULL != m_pShaderManager)
	{
		m_pShaderManager->setMat4Value(g_ModelName, modelView);
	}
}

/***********************************************************
 *  SetShaderColor()
 ***********************************************************/
void SceneManager::SetShaderColor(
	float redColorValue,
	float greenColorValue,
	float blueColorValue,
	float alphaValue)
{
	glm::vec4 currentColor;
	currentColor.r = redColorValue;
	currentColor.g = greenColorValue;
	currentColor.b = blueColorValue;
	currentColor.a = alphaValue;

	if (NULL != m_pShaderManager)
	{
		m_pShaderManager->setIntValue(g_UseTextureName, false);
		m_pShaderManager->setVec4Value(g_ColorValueName, currentColor);
	}
}

/***********************************************************
 *  SetShaderTexture()
 ***********************************************************/
void SceneManager::SetShaderTexture(std::string textureTag)
{
	if (NULL != m_pShaderManager)
	{
		m_pShaderManager->setIntValue(g_UseTextureName, true);

		int textureSlot = FindTextureSlot(textureTag);
		m_pShaderManager->setSampler2DValue(g_TextureValueName, textureSlot);
	}
}

/***********************************************************
 *  SetTextureUVScale()
 ***********************************************************/
void SceneManager::SetTextureUVScale(float u, float v)
{
	if (NULL != m_pShaderManager)
	{
		m_pShaderManager->setVec2Value("UVscale", glm::vec2(u, v));
	}
}

/***********************************************************
 *  SetShaderMaterial()
 ***********************************************************/
void SceneManager::SetShaderMaterial(std::string materialTag)
{
	if (m_objectMaterials.size() > 0 && NULL != m_pShaderManager)
	{
		OBJECT_MATERIAL material;
		bool bFound = FindMaterial(materialTag, material);

		if (bFound == true)
		{
			m_pShaderManager->setVec3Value("material.diffuseColor", material.diffuseColor);
			m_pShaderManager->setVec3Value("material.specularColor", material.specularColor);
			m_pShaderManager->setFloatValue("material.shininess", material.shininess);
		}
	}
}

/**************************************************************/
/*** STUDENTS CAN MODIFY the code in the methods BELOW       ***/
/**************************************************************/

/***********************************************************
 *  PrepareScene()
 ***********************************************************/
void SceneManager::PrepareScene()
{
	// load meshes once
	m_basicMeshes->LoadPlaneMesh();
	m_basicMeshes->LoadBoxMesh();
	m_basicMeshes->LoadCylinderMesh();
	m_basicMeshes->LoadSphereMesh();

	// load textures once
	CreateGLTexture("textures/Wood.jpg", "wood");
	CreateGLTexture("textures/Plastic.jpg", "plastic");
	CreateGLTexture("textures/Keyboard.jpg", "keyboard");
	CreateGLTexture("textures/monitorscreen.jpg", "monitorscreen");
	CreateGLTexture("textures/DarkGrey.jpg", "DarkGrey");
	CreateGLTexture("textures/Mouse.png", "mouse");
	CreateGLTexture("textures/logitech.jpg", "logitech");
	CreateGLTexture("textures/Plant.jpg", "plant");
	CreateGLTexture("textures/Pot.jpg", "pot");

	// bind texture IDs to texture slots
	BindGLTextures();

	// -----------------------------
	// Materials for lighting
	// -----------------------------
	m_objectMaterials.clear();

	OBJECT_MATERIAL woodMat;
	woodMat.tag = "woodMat";
	woodMat.diffuseColor = glm::vec3(1.0f, 1.0f, 1.0f);
	woodMat.specularColor = glm::vec3(0.20f, 0.20f, 0.20f);
	woodMat.shininess = 16.0f;
	m_objectMaterials.push_back(woodMat);

	OBJECT_MATERIAL plasticMat;
	plasticMat.tag = "plasticMat";
	plasticMat.diffuseColor = glm::vec3(1.0f, 1.0f, 1.0f);
	plasticMat.specularColor = glm::vec3(0.15f, 0.15f, 0.15f);
	plasticMat.shininess = 8.0f;
	m_objectMaterials.push_back(plasticMat);

	OBJECT_MATERIAL keyboardMat;
	keyboardMat.tag = "keyboardMat";
	keyboardMat.diffuseColor = glm::vec3(1.0f, 1.0f, 1.0f);
	keyboardMat.specularColor = glm::vec3(0.50f, 0.50f, 0.50f);
	keyboardMat.shininess = 48.0f;
	m_objectMaterials.push_back(keyboardMat);
}

/***********************************************************
 *  SetupSceneLights()
 *
 *  Called once per frame from RenderScene()
 ***********************************************************/
void SceneManager::SetupSceneLights()
{
	if (m_pShaderManager == NULL)
		return;

	// Turn lighting ON in fragment shader
	m_pShaderManager->setBoolValue(g_UseLightingName, true);

	// ---------------------------
	// Directional Light (main)
	// ---------------------------
	m_pShaderManager->setVec3Value("directionalLight.direction", -0.25f, -1.0f, -0.30f);
	m_pShaderManager->setVec3Value("directionalLight.ambient", 0.35f, 0.35f, 0.35f);
	m_pShaderManager->setVec3Value("directionalLight.diffuse", 0.70f, 0.70f, 0.70f);
	m_pShaderManager->setVec3Value("directionalLight.specular", 0.60f, 0.60f, 0.60f);
	m_pShaderManager->setBoolValue("directionalLight.bActive", true);

	// ---------------------------
	// Point Lights (fill lights)
	// TOTAL_POINT_LIGHTS = 5 in shader
	// ---------------------------
	// Light 0: above/right
	m_pShaderManager->setVec3Value("pointLights[0].position", 3.0f, 3.0f, 2.0f);
	m_pShaderManager->setVec3Value("pointLights[0].ambient", 0.06f, 0.06f, 0.06f);
	m_pShaderManager->setVec3Value("pointLights[0].diffuse", 0.80f, 0.80f, 0.80f);
	m_pShaderManager->setVec3Value("pointLights[0].specular", 0.90f, 0.90f, 0.90f);
	m_pShaderManager->setBoolValue("pointLights[0].bActive", true);

	// Light 1: fill from opposite side (prevents full shadow)
	m_pShaderManager->setVec3Value("pointLights[1].position", -3.0f, 2.5f, -2.0f);
	m_pShaderManager->setVec3Value("pointLights[1].ambient", 0.05f, 0.05f, 0.05f);
	m_pShaderManager->setVec3Value("pointLights[1].diffuse", 0.45f, 0.45f, 0.45f);
	m_pShaderManager->setVec3Value("pointLights[1].specular", 0.50f, 0.50f, 0.50f);
	m_pShaderManager->setBoolValue("pointLights[1].bActive", true);

	// Light 2: soft overhead fill (makes the scene look more real)
	m_pShaderManager->setVec3Value("pointLights[2].position", 0.0f, 4.0f, 0.0f);
	m_pShaderManager->setVec3Value("pointLights[2].ambient", 0.03f, 0.03f, 0.03f);
	m_pShaderManager->setVec3Value("pointLights[2].diffuse", 0.35f, 0.35f, 0.35f);
	m_pShaderManager->setVec3Value("pointLights[2].specular", 0.20f, 0.20f, 0.20f);
	m_pShaderManager->setBoolValue("pointLights[2].bActive", true);

	// Disable unused point lights
	for (int i = 3; i < 5; i++)
	{
		std::string base = "pointLights[" + std::to_string(i) + "].bActive";
		m_pShaderManager->setBoolValue(base, false);
	}

	// Spotlight off for this scene
	m_pShaderManager->setBoolValue("spotLight.bActive", false);
}

/***********************************************************
 *  RenderScene()
 ***********************************************************/
void SceneManager::RenderScene()
{
	// set lights once per frame
	SetupSceneLights();

	// declare the variables for the transformations
	glm::vec3 scaleXYZ;
	float XrotationDegrees = 0.0f;
	float YrotationDegrees = 0.0f;
	float ZrotationDegrees = 0.0f;
	glm::vec3 positionXYZ;

	/******************************************************************/
	// Floor plane (wood texture + lit)
	/******************************************************************/
	scaleXYZ = glm::vec3(20.0f, 1.0f, 10.0f);
	positionXYZ = glm::vec3(0.0f, 0.0f, 0.0f);

	SetTransformations(scaleXYZ, 0.0f, 0.0f, 0.0f, positionXYZ);
	SetShaderMaterial("woodMat");
	SetShaderTexture("wood");
	SetTextureUVScale(8.0f, 4.0f);
	m_basicMeshes->DrawPlaneMesh();

	/******************************************************************/
	// Desk plane (plastic texture + lit)
	/******************************************************************/
	scaleXYZ = glm::vec3(12.0f, 1.0f, 6.0f);
	positionXYZ = glm::vec3(0.0f, 0.05f, 0.0f);

	SetTransformations(scaleXYZ, 0.0f, 0.0f, 0.0f, positionXYZ);
	SetShaderMaterial("plasticMat");
	SetShaderTexture("plastic");
	SetTextureUVScale(1.0f, 1.0f);
	m_basicMeshes->DrawPlaneMesh();

	/******************************************************************/
	// Keyboard base (color only + lit)
	/******************************************************************/
	scaleXYZ = glm::vec3(6.5f, 0.35f, 2.2f);
	positionXYZ = glm::vec3(0.0f, 0.20f, -0.40f);

	SetTransformations(scaleXYZ, 0.0f, 0.0f, 0.0f, positionXYZ);
	SetShaderMaterial("plasticMat");
	SetShaderTexture("keyboard");

	// DARKEN the texture so keys fade away
	SetShaderColor(0.15f, 0.15f, 0.15f, 1.0f);

	SetTextureUVScale(1.0f, 1.0f);
	m_basicMeshes->DrawBoxMeshSide(ShapeMeshes::box_front);
	m_basicMeshes->DrawBoxMeshSide(ShapeMeshes::box_back);
	m_basicMeshes->DrawBoxMeshSide(ShapeMeshes::box_left);
	m_basicMeshes->DrawBoxMeshSide(ShapeMeshes::box_right);
	m_basicMeshes->DrawBoxMeshSide(ShapeMeshes::box_bottom);
	m_basicMeshes->DrawBoxMeshSide(ShapeMeshes::box_top);

	/******************************************************************/
	// Keyboard top plate
	/******************************************************************/
	scaleXYZ = glm::vec3(6.3f, 0.12f, 2.0f);
	positionXYZ = glm::vec3(0.0f, 0.20f + (0.35f / 2.0f) + (0.12f / 2.0f), -0.40f);

	SetTransformations(scaleXYZ, 0.0f, 0.0f, 0.0f, positionXYZ);

	// TOP face = keyboard texture
	SetShaderMaterial("keyboardMat");
	SetShaderTexture("keyboard");
	SetTextureUVScale(1.0f, 1.0f);
	m_basicMeshes->DrawBoxMeshSide(ShapeMeshes::box_top);

	// All other faces
	SetShaderMaterial("plasticMat");
	SetShaderTexture("plastic");
	SetTextureUVScale(1.0f, 1.0f);
	m_basicMeshes->DrawBoxMeshSide(ShapeMeshes::box_front);
	m_basicMeshes->DrawBoxMeshSide(ShapeMeshes::box_back);
	m_basicMeshes->DrawBoxMeshSide(ShapeMeshes::box_left);
	m_basicMeshes->DrawBoxMeshSide(ShapeMeshes::box_right);
	m_basicMeshes->DrawBoxMeshSide(ShapeMeshes::box_bottom);

	/******************************************************************/
	// Monitor stand (cylinder)
	/******************************************************************/
	float deskY = 0.05f;

	scaleXYZ = glm::vec3(0.18f, 1.6f, 0.18f);
	positionXYZ = glm::vec3(0.0f, deskY, -2.0f);

	SetTransformations(scaleXYZ, 0.0f, 0.0f, 0.0f, positionXYZ);
	SetShaderMaterial("plasticMat");
	SetShaderTexture("DarkGrey");
	SetTextureUVScale(1.0f, 1.0f);
	m_basicMeshes->DrawCylinderMesh();

	/******************************************************************/
	// Monitor (box)
	/******************************************************************/
	scaleXYZ = glm::vec3(7.2f, 3.4f, 0.18f);

	float monitorHalfH = scaleXYZ.y * 0.5f;

	positionXYZ = glm::vec3(0.0f, deskY + 1.2f + monitorHalfH, -2.0f);

	SetTransformations(scaleXYZ, -10.0f, 0.0f, 0.0f, positionXYZ);
	SetShaderMaterial("plasticMat");
	SetShaderTexture("monitorscreen");
	SetTextureUVScale(1.0f, 1.0f);

	// SCREEN FACE (no lighting — pure texture)
	m_pShaderManager->setBoolValue(g_UseLightingName, false);
	SetShaderTexture("monitorscreen");
	m_basicMeshes->DrawBoxMeshSide(ShapeMeshes::box_front);

	// Everything else = plastic (frame/back)
	SetShaderTexture("DarkGrey");
	m_basicMeshes->DrawBoxMeshSide(ShapeMeshes::box_back);
	m_basicMeshes->DrawBoxMeshSide(ShapeMeshes::box_left);
	m_basicMeshes->DrawBoxMeshSide(ShapeMeshes::box_right);
	m_basicMeshes->DrawBoxMeshSide(ShapeMeshes::box_top);
	m_basicMeshes->DrawBoxMeshSide(ShapeMeshes::box_bottom);

	// Turn lighting back ON
	m_pShaderManager->setBoolValue(g_UseLightingName, true);

	/******************************************************************/
	// Mousepad
	/******************************************************************/
	glm::vec3 padScale = glm::vec3(3.2f, 0.05f, 2.4f);
	glm::vec3 padPos = glm::vec3(5.8f, deskY + (padScale.y * 0.5f) + 0.01f, -0.35f);

	SetTransformations(padScale, 0.0f, 0.0f, 0.0f, padPos);

	// TOP FACE (logitech texture)
	SetShaderMaterial("plasticMat");
	SetShaderColor(1.0f, 1.0f, 1.0f, 1.0f);
	SetShaderTexture("logitech");
	SetTextureUVScale(1.0f, 1.0f);
	m_basicMeshes->DrawBoxMeshSide(ShapeMeshes::box_top);

	// ALL OTHER SIDES (dark grey)
	SetShaderTexture("DarkGrey");
	SetTextureUVScale(1.0f, 1.0f);
	m_basicMeshes->DrawBoxMeshSide(ShapeMeshes::box_front);
	m_basicMeshes->DrawBoxMeshSide(ShapeMeshes::box_back);
	m_basicMeshes->DrawBoxMeshSide(ShapeMeshes::box_left);
	m_basicMeshes->DrawBoxMeshSide(ShapeMeshes::box_right);
	m_basicMeshes->DrawBoxMeshSide(ShapeMeshes::box_bottom);

	/******************************************************************/
	// Mouse
	/******************************************************************/
	float padTopY = padPos.y + (padScale.y * 0.5f);

	// Size of the mouse 
	float mouseR = 0.75f;
	glm::vec3 mouseScale = glm::vec3(mouseR, mouseR * 0.55f, mouseR);

	// Position: sit on pad
	glm::vec3 mousePos = glm::vec3(
		padPos.x,
		padTopY + (mouseScale.y * 0.35f),
		padPos.z
	);

	SetTransformations(mouseScale, 0.0f, 200.0f, 0.0f, mousePos);

	//mouse texture
	SetShaderMaterial("plasticMat");
	SetShaderColor(1.0f, 1.0f, 1.0f, 1.0f);
	
	SetTextureUVScale(0.90f, 0.70f);
	SetShaderTexture("mouse");
	SetTextureUVScale(1.0f, 1.0f);
	m_basicMeshes->DrawSphereMesh();
	m_basicMeshes->DrawBoxMeshSide(ShapeMeshes::box_top);

	/******************************************************************/
	// Desk Plant (LEFT) - Pot
	/******************************************************************/

		float plantX = -5.2f;
		float plantZ = -0.35f;

		// --- Pot (cylinder) ---
		glm::vec3 potScale = glm::vec3(0.55f, 0.45f, 0.55f);
		glm::vec3 potPos = glm::vec3(plantX, deskY + (potScale.y * 0.5f), plantZ);

		SetTransformations(potScale, 0.0f, 0.0f, 0.0f, potPos);
		SetShaderMaterial("plasticMat");
		SetShaderColor(1.0f, 1.0f, 1.0f, 1.0f);
		SetShaderTexture("pot");
		SetTextureUVScale(1.0f, 1.0f);
		m_basicMeshes->DrawCylinderMesh();

		//soil
		SetShaderTexture("soil");
		SetTextureUVScale(1.0f, 1.0f);
		m_basicMeshes->DrawSphereMesh();
		m_basicMeshes->DrawBoxMeshSide(ShapeMeshes::box_top);

		// Where the plant starts (top of pot)
		float leavesBaseY = potPos.y + (potScale.y * 0.5f) + 0.02f;

		// center for the plant
		float cx = plantX;
		float cz = plantZ;

		// --- Stem (skinny cylinder) ---
		glm::vec3 stemScale = glm::vec3(0.08f, 0.70f, 0.08f);
		glm::vec3 stemPos = glm::vec3(cx, leavesBaseY + (stemScale.y * 0.5f) - 0.12f, cz);

		SetTransformations(stemScale, 0.0f, 0.0f, 0.0f, stemPos);
		SetShaderMaterial("plasticMat");
		SetShaderColor(0.35f, 0.28f, 0.20f, 1.0f);
		m_pShaderManager->setIntValue(g_UseTextureName, false);
		m_basicMeshes->DrawCylinderMesh();

		// --- Leaves (oval spheres) ---
		SetShaderMaterial("plasticMat");
		SetShaderColor(1.0f, 1.0f, 1.0f, 1.0f);
		SetShaderTexture("plant");
		SetTextureUVScale(1.0f, 1.0f);

		glm::vec3 leafScale = glm::vec3(0.22f, 0.07f, 0.16f);

		auto DrawLeaf = [&](glm::vec3 pos, float xRot, float yRot, float zRot, glm::vec3 scaleOverride)
			{
				SetTransformations(scaleOverride, xRot, yRot, zRot, pos);
				m_basicMeshes->DrawSphereMesh();
			};

		auto DrawLeafDefault = [&](glm::vec3 pos, float xRot, float yRot, float zRot)
			{
				DrawLeaf(pos, xRot, yRot, zRot, leafScale);
			};

		// Heights
		float yBottom = leavesBaseY + 0.28f;
		float yMid = leavesBaseY + 0.45f;
		float yTop = leavesBaseY + 0.58f;

		// -------- Enhanced Top Crown --------
		glm::vec3 capScale = glm::vec3(0.16f, 0.05f, 0.12f);
		float yCap = leavesBaseY + 0.80f;
		float rCap = 0.09f;

		// 8 leaves around tip
		DrawLeaf(glm::vec3(cx + rCap, yCap, cz), 40.0f, 90.0f, 0.0f, capScale);
		DrawLeaf(glm::vec3(cx - rCap, yCap, cz), 40.0f, -90.0f, 0.0f, capScale);
		DrawLeaf(glm::vec3(cx, yCap, cz + rCap), 42.0f, 0.0f, 0.0f, capScale);
		DrawLeaf(glm::vec3(cx, yCap, cz - rCap), 35.0f, 180.0f, 0.0f, capScale);

		DrawLeaf(glm::vec3(cx + 0.07f, yCap, cz + 0.07f), 45.0f, 45.0f, 0.0f, capScale);
		DrawLeaf(glm::vec3(cx - 0.07f, yCap, cz + 0.07f), 45.0f, -45.0f, 0.0f, capScale);
		DrawLeaf(glm::vec3(cx + 0.07f, yCap, cz - 0.07f), 35.0f, 135.0f, 0.0f, capScale);
		DrawLeaf(glm::vec3(cx - 0.07f, yCap, cz - 0.07f), 35.0f, -135.0f, 0.0f, capScale);

		// tiny top leaf
		DrawLeaf(glm::vec3(cx, yCap + 0.04f, cz), 90.0f, 0.0f, 0.0f, glm::vec3(0.13f, 0.04f, 0.13f));

		// -------- Bottom ring --------
		float rB = 0.20f;
		DrawLeafDefault(glm::vec3(cx + rB, yBottom, cz), 0.0f, 90.0f, 0.0f);
		DrawLeafDefault(glm::vec3(cx - rB, yBottom, cz), 0.0f, -90.0f, 0.0f);
		DrawLeafDefault(glm::vec3(cx, yBottom, cz + rB), 10.0f, 0.0f, 0.0f);
		DrawLeafDefault(glm::vec3(cx, yBottom, cz - rB), -10.0f, 180.0f, 0.0f);
		DrawLeafDefault(glm::vec3(cx + 0.14f, yBottom, cz + 0.14f), 8.0f, 45.0f, 0.0f);
		DrawLeafDefault(glm::vec3(cx - 0.14f, yBottom, cz + 0.14f), 8.0f, -45.0f, 0.0f);

		// -------- Middle ring --------
		float rM = 0.17f;
		DrawLeafDefault(glm::vec3(cx + rM, yMid, cz + 0.02f), 5.0f, 80.0f, 0.0f);
		DrawLeafDefault(glm::vec3(cx - rM, yMid, cz + 0.02f), 5.0f, -80.0f, 0.0f);
		DrawLeafDefault(glm::vec3(cx + 0.02f, yMid, cz + rM), 12.0f, 0.0f, 0.0f);
		DrawLeafDefault(glm::vec3(cx - 0.02f, yMid, cz - rM), -8.0f, 180.0f, 0.0f);
		DrawLeafDefault(glm::vec3(cx, yMid + 0.01f, cz), 25.0f, 20.0f, 0.0f);

		// -------- Top ring --------
		glm::vec3 leafScaleTop = glm::vec3(0.18f, 0.06f, 0.13f);
		float rT = 0.12f;

		DrawLeaf(glm::vec3(cx + rT, yTop, cz), 28.0f, 90.0f, 0.0f, leafScaleTop);
		DrawLeaf(glm::vec3(cx - rT, yTop, cz), 28.0f, -90.0f, 0.0f, leafScaleTop);
		DrawLeaf(glm::vec3(cx, yTop, cz + rT), 30.0f, 0.0f, 0.0f, leafScaleTop);
		DrawLeaf(glm::vec3(cx, yTop, cz - rT), 22.0f, 180.0f, 0.0f, leafScaleTop);
	}