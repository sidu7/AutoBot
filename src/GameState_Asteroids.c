//Start Header -------------------------------------------------------
//
//Copyright (C) 2018 DigiPen Institute of Technology.
//Reproduction or disclosure of this file or its contents without the prior
//written consent of DigiPen Institute
//of Technology is prohibited.
//File Name:		GameState_Asteroids.c
//Purpose:			Application of Fuzzy Logic in game
//Language:			C
//Platform:			Visual Studio 2017 | Visual C++ 14.1 | Windows 10 Home
//Project:			MAT562_FuzzyLogics_Project
//Author:			Sidhant Tumma
//Creation date:	10/25/2018
//
//- End Header --------------------------------------------------------

#include "main.h"
#include "Matrix2D.h"
#include "Math2D.h"
#include "Vector2D.h"
#include <time.h>
// ---------------------------------------------------------------------------
// Defines

#define SHAPE_NUM_MAX				32					// The total number of different vertex buffer (Shape)
#define GAME_OBJ_INST_NUM_MAX		2048				// The total number of different game object instances

#define TEXTURE_NUM_MAX				32					// The total number of different textures (*)

// Feel free to change these values in ordet to make the game more fun
#define SHIP_INITIAL_NUM			3					// Initial number of ship lives
#define SHIP_SIZE					60.0f				// Ship size
#define BULLET_SPEED				700.0f				// Bullet speed (m/s)

// ---------------------------------------------------------------------------
// ---------------------------------------------------------------------------

enum OBJECT_TYPE
{
	// list of game object types
	OBJECT_TYPE_SHIP = 0,
	OBJECT_TYPE_BOT,
	OBJECT_TYPE_PLAYER_BULLET,
	OBJECT_TYPE_BOT_BULLET,	
	OBJECT_TYPE_NUM
};

// ---------------------------------------------------------------------------
// object mFlag definition

#define FLAG_ACTIVE		0x00000001

// ---------------------------------------------------------------------------
// Struct/Class definitions

typedef struct GameObjectInstance GameObjectInstance;			// Forward declaration needed, since components need to point to their owner "GameObjectInstance"

// ---------------------------------------------------------------------------

typedef struct 
{
	unsigned long			mType;				// Object type (Ship, bullet, etc..)
	AEGfxVertexList*		mpMesh;				// This will hold the triangles which will form the shape of the object

}Shape;

// ---------------------------------------------------------------------------

typedef struct
{
	Shape *mpShape;

	GameObjectInstance *	mpOwner;			// This component's owner
}Component_Sprite;

// ---------------------------------------------------------------------------

typedef struct
{
	Vector2D				mPosition;			// Current position
	float					mAngle;				// Current angle
	float					mScaleX;			// Current X scaling value
	float					mScaleY;			// Current Y scaling value

	Matrix2D				mTransform;			// Object transformation matrix: Each frame, calculate the object instance's transformation matrix and save it here

	GameObjectInstance *	mpOwner;			// This component's owner
}Component_Transform;

// ---------------------------------------------------------------------------

typedef struct
{
	Vector2D					mVelocity;			// Current velocity

	GameObjectInstance *	mpOwner;			// This component's owner
}Component_Physics;

// ---------------------------------------------------------------------------

typedef struct
{
	GameObjectInstance *		mpTarget;		// Target, used by the homing missile

	GameObjectInstance *		mpOwner;		// This component's owner
}Component_Target;

// ---------------------------------------------------------------------------


//Game object instance structure
struct GameObjectInstance
{
	unsigned long				mFlag;						// Bit mFlag, used to indicate if the object instance is active or not

	Component_Sprite			*mpComponent_Sprite;		// Sprite component
	Component_Transform			*mpComponent_Transform;		// Transform component
	Component_Physics			*mpComponent_Physics;		// Physics component
	Component_Target			*mpComponent_Target;		// Target component, used by the homing missile
};

// ---------------------------------------------------------------------------
// Static variables

// List of original vertex buffers
static Shape					sgShapes[SHAPE_NUM_MAX];									// Each element in this array represents a unique shape 
static unsigned long			sgShapeNum;													// The number of defined shapes

// list of object instances
static GameObjectInstance		sgGameObjectInstanceList[GAME_OBJ_INST_NUM_MAX];		// Each element in this array represents a unique game object instance
static unsigned long			sgGameObjectInstanceNum;								// The number of active game object instances

// pointer ot the ship object
static GameObjectInstance*		sgpShip;												// Pointer to the "Ship" game object instance
static GameObjectInstance*		sgpBot;													// Pointer to the "Bot"

// number of ship available (lives 0 = game over)
static int						sgShipLives;											// The number of lives left
static int						sgBotLives;

// the score = number of asteroid destroyed
static unsigned long			sgPlayerScore;												// Current score
static unsigned long			sgBotScore;
// --------------------------------------------------------------------------

// functions to create/destroy a game object instance
static GameObjectInstance*			GameObjectInstanceCreate(unsigned int ObjectType);			// From OBJECT_TYPE enum
static void							GameObjectInstanceDestroy(GameObjectInstance* pInst);

// ---------------------------------------------------------------------------

// Functions to add/remove components
static void AddComponent_Transform(GameObjectInstance *pInst, Vector2D *pPosition, float Angle, float ScaleX, float ScaleY);
static void AddComponent_Sprite(GameObjectInstance *pInst, unsigned int ShapeType);
static void AddComponent_Physics(GameObjectInstance *pInst, Vector2D *pVelocity);
static void AddComponent_Target(GameObjectInstance *pInst, GameObjectInstance *pTarget);

static void RemoveComponent_Transform(GameObjectInstance *pInst);
static void RemoveComponent_Sprite(GameObjectInstance *pInst);
static void RemoveComponent_Physics(GameObjectInstance *pInst);
static void RemoveComponent_Target(GameObjectInstance *pInst);

// ---------------------------------------------------------------------------

static float ShipX = 0.0f;
static float ShipY = -150.0f;
static float BotX = 0.0f;
static float BotY = 150.0f;
static float PlayerCHP = 100.0f;
static float BotCHP = 100.0f;
static int PlayerCAmmo = 5;
static int BotCAmmo = 5;
static float toMoveX = 0.0f;
static float toMoveY = 0.0f;

enum Textures {
	LIVES = 1,
	AMMO,
};

static AEGfxTexture*			sgTextures[TEXTURE_NUM_MAX];
static AEGfxVertexList*			Lives;
static AEGfxVertexList*			Ammo;

static float FindPlayer(Vector2D DirVec, Vector2D BotVec);
static float getFuzzyOutputY(float BotHP, float BotAmmo, float PlayerPos);
static float Triangle(float a, float b, float c, float x);

static double timerP, timerB;

// --------------------------------------------------------------------------

// "Load" function of this state
void GameStateAsteroidsLoad(void)
{
	Shape* pShape = NULL;

	// Zero the shapes array
	memset(sgShapes, 0, sizeof(Shape) * SHAPE_NUM_MAX);
	// No shapes at this point
	sgShapeNum = 0;

	//Zero the textures array
	memset(sgTextures, 0, sizeof(AEGfxTexture*) * TEXTURE_NUM_MAX);


	// The ship object instance hasn't been created yet, so this "sgpShip" pointer is initialized to 0
	sgpShip = 0;
	sgpBot = 0;

	// Create the game objects(shapes) : Ships, Bullet, Asteroid and Missile
	// How to:
	// -- Remember to create normalized shapes, which means all the vertices' coordinates should be in the [-0.5;0.5] range. Use the object instances' scale values to resize the shape.
	// -- Call “AEGfxMeshStart” to inform the graphics manager that you are about the start sending triangles.
	// -- Call “AEGfxTriAdd” to add 1 triangle.
	// -- A triangle is formed by 3 counter clockwise vertices (points).
	// -- Create all the points between (-0.5, -0.5) and (0.5, 0.5), and use the object instance's scale to change the size.
	// -- Each point can have its own color.
	// -- The color format is : ARGB, where each 2 hexadecimal digits represent the value of the Alpha, Red, Green and Blue respectively. Note that alpha blending(Transparency) is not implemented.
	// -- Each point can have its own texture coordinate (set them to 0.0f in case you’re not applying a texture).
	// -- An object (Shape) can have multiple triangles.
	// -- Call “AEGfxMeshEnd” to inform the graphics manager that you are done creating a mesh, which will return a pointer to the mesh you just created.

	// Player Ship
	pShape = sgShapes + sgShapeNum++;
	pShape->mType = OBJECT_TYPE_SHIP;

	AEGfxMeshStart();
	AEGfxTriAdd(
		-0.5f,  0.5f, 0xFFFFFFFF, 0.0f, 0.0f,
		-0.5f, -0.5f, 0xFFFFFFFF, 0.0f, 0.0f,
		 0.5f, -0.5f, 0xFFFFFFFF, 0.0f, 0.0f);
	AEGfxTriAdd(
		 0.5f, -0.5f, 0xFFFFFFFF, 0.0f, 0.0f,
		 0.5f,  0.5f, 0xFFFFFFFF, 0.0f, 0.0f,
		-0.5f,  0.5f, 0xFFFFFFFF, 0.0f, 0.0f);
	pShape->mpMesh = AEGfxMeshEnd();
	

	// Bot Ship
	pShape = sgShapes + sgShapeNum++;
	pShape->mType = OBJECT_TYPE_BOT;
	
	AEGfxMeshStart();
	AEGfxTriAdd(
		-0.5f, 0.5f, 0xFFFFFFFF, 0.0f, 0.0f,
		-0.5f, -0.5f, 0xFFFFFFFF, 0.0f, 0.0f,
		0.5f, -0.5f, 0xFFFFFFFF, 0.0f, 0.0f);
	AEGfxTriAdd(
		0.5f, -0.5f, 0xFFFFFFFF, 0.0f, 0.0f,
		0.5f, 0.5f, 0xFFFFFFFF, 0.0f, 0.0f,
		-0.5f, 0.5f, 0xFFFFFFFF, 0.0f, 0.0f);
	pShape->mpMesh = AEGfxMeshEnd();

	
	// Player Bullet
	pShape = sgShapes + sgShapeNum++;
	pShape->mType = OBJECT_TYPE_PLAYER_BULLET;

	AEGfxMeshStart();
	AEGfxTriAdd(
		-0.5f,  0.5f, 0xFFFFFFFF, 0.0f, 0.0f,
		-0.5f, -0.5f, 0xFFFFFFFF, 0.0f, 0.0f,
		 0.5f,  0.5f, 0xFFFFFFFF, 0.0f, 0.0f);
	AEGfxTriAdd(
		 0.5f,  0.5f, 0xFFFFFFFF, 0.0f, 0.0f,
		-0.5f, -0.5f, 0xFFFFFFFF, 0.0f, 0.0f,
		 0.5f, -0.5f, 0xFFFFFFFF, 0.0f, 0.0f);
	pShape->mpMesh = AEGfxMeshEnd();

	// Bot Bullet
	pShape = sgShapes + sgShapeNum++;
	pShape->mType = OBJECT_TYPE_BOT_BULLET;

	AEGfxMeshStart();
	AEGfxTriAdd(
		-0.5f, 0.5f, 0xFFFFFFFF, 0.0f, 0.0f,
		-0.5f, -0.5f, 0xFFFFFFFF, 0.0f, 0.0f,
		0.5f, 0.5f, 0xFFFFFFFF, 0.0f, 0.0f);
	AEGfxTriAdd(
		0.5f, 0.5f, 0xFFFFFFFF, 0.0f, 0.0f,
		-0.5f, -0.5f, 0xFFFFFFFF, 0.0f, 0.0f,
		0.5f, -0.5f, 0xFFFFFFFF, 0.0f, 0.0f);
	pShape->mpMesh = AEGfxMeshEnd();

	//Lives
	AEGfxMeshStart();

	AEGfxTriAdd(
		-7.0f, -7.0f, 0x00FF00FF, 0.0f, 1.0f,
		7.0f, -7.0f, 0x00FFFF00, 1.0f, 1.0f,
		-7.0f, 7.0f, 0x00F00FFF, 0.0f, 0.0f);

	AEGfxTriAdd(
		7.0f, -7.0f, 0x00FFFFFF, 1.0f, 1.0f,
		7.0f, 7.0f, 0x00FFFFFF, 1.0f, 0.0f,
		-7.0f, 7.0f, 0x00FFFFFF, 0.0f, 0.0f);

	Lives = AEGfxMeshEnd();
	sgTextures[LIVES] = AEGfxTextureLoad("Textures/lives.png");

	//Ammo
	AEGfxMeshStart();

	AEGfxTriAdd(
		-7.0f, -3.0f, 0x00FF00FF, 0.0f, 1.0f,
		7.0f, -3.0f, 0x00FFFF00, 1.0f, 1.0f,
		-7.0f, 3.0f, 0x00F00FFF, 0.0f, 0.0f);

	AEGfxTriAdd(
		7.0f, -3.0f, 0x00FFFFFF, 1.0f, 1.0f,
		7.0f, 3.0f, 0x00FFFFFF, 1.0f, 0.0f,
		-7.0f, 3.0f, 0x00FFFFFF, 0.0f, 0.0f);

	Ammo = AEGfxMeshEnd();
	sgTextures[AMMO] = AEGfxTextureLoad("Textures/bullet.png");
}

// ---------------------------------------------------------------------------

// "Initialize" function of this state
void GameStateAsteroidsInit(void)
{
	AEGfxSetBackgroundColor(0.0f, 0.0f, 0.0f);
	AEGfxSetBlendMode(AE_GFX_BM_BLEND);

	// zero the game object instance array
	memset(sgGameObjectInstanceList, 0, sizeof(GameObjectInstance)* GAME_OBJ_INST_NUM_MAX);
	// No game object instances (sprites) at this point
	sgGameObjectInstanceNum = 0;

	//timers
	timerP = 0.0f;
	timerB = 0.0f;

	// create the player ship
	sgpShip = GameObjectInstanceCreate(OBJECT_TYPE_SHIP);
	sgpShip->mpComponent_Transform->mScaleX = SHIP_SIZE;
	sgpShip->mpComponent_Transform->mScaleY = SHIP_SIZE;
	sgpShip->mpComponent_Transform->mAngle = PI/2;
	Vector2DSet(&sgpShip->mpComponent_Transform->mPosition, ShipX, ShipY);
	// Create Player HP and Ammo
	for (int i = 0; i < 4; i++)
	{
		//Vector2DSet(&PlayerHP[i]->mpComponent_Transform->mPosition, ShipX + (-22 + i*15), ShipY - 40.0f);
	}
	for (int i = 0; i < 5; i++)
	{
		//Vector2DSet(&PlayerAmmo[i]->mpComponent_Transform->mPosition, ShipX + 40.0f, ShipY + (25 - i * 12));
	}
	
	// create the Bot ship
	sgpBot = GameObjectInstanceCreate(OBJECT_TYPE_BOT);
	sgpBot->mpComponent_Transform->mScaleX = SHIP_SIZE;
	sgpBot->mpComponent_Transform->mScaleY = SHIP_SIZE;
	sgpBot->mpComponent_Transform->mAngle = -PI / 2;
	Vector2DSet(&sgpBot->mpComponent_Transform->mPosition, BotX, BotY);
	// Create Bot HP and Ammo
	for (int i = 0; i < 4; i++)
	{
		//Vector2DSet(&BotHP[i]->mpComponent_Transform->mPosition, BotX + (-22 + i * 15), BotY + 40.0f);
	}
	for (int i = 0; i < 5; i++)
	{
		//Vector2DSet(&BotAmmo[i]->mpComponent_Transform->mPosition, BotX - 40.0f, BotY + (25 - i * 12));
	}

	sgShipLives = 4;
	sgBotLives = 4;

	sgBotScore = 0;
	sgPlayerScore = 0;
}

// ---------------------------------------------------------------------------

// "Update" function of this state
void GameStateAsteroidsUpdate(void)
{
	unsigned long i;
	float winMaxX, winMaxY, winMinX, winMinY;
	double frameTime;

	// ==========================================================================================
	// Getting the window's world edges (These changes whenever the camera moves or zooms in/out)
	// ==========================================================================================
	winMaxX = AEGfxGetWinMaxX();
	winMaxY = AEGfxGetWinMaxY();
	winMinX = AEGfxGetWinMinX();
	winMinY = AEGfxGetWinMinY();
	
	
	// ======================
	// Getting the frame time
	// ======================

	frameTime = AEFrameRateControllerGetFrameTime();
	timerP += frameTime;
	timerB += frameTime;

	// =========================
	// Update according to input
	// =========================

	/////////////////////////////////////////////////////////////////////////////////////////////////
	/////////////////////////////////////////////////////////////////////////////////////////////////
	// TO DO 3:
	// -- Compute the forward/backward acceleration of the ship when Up/Down are pressed
	// -- Use the acceleration to update the velocity of the ship
	// -- Limit the maximum velocity of the ship
	// -- IMPORTANT: The current input code moves the ship by simply adjusting its position
	/////////////////////////////////////////////////////////////////////////////////////////////////
	/////////////////////////////////////////////////////////////////////////////////////////////////
	if (AEInputCheckCurr(VK_UP))
	{
		ShipY += 5.0f;
		if (ShipY > - SHIP_SIZE)
		{
			ShipY = -SHIP_SIZE;
		}
		sgpShip->mpComponent_Transform->mPosition.y = ShipY;
	}

	if (AEInputCheckCurr(VK_DOWN))
	{
		ShipY -= 5.0f;
		if (ShipY < (int)winMinY + SHIP_SIZE)
		{
			ShipY = winMinY + SHIP_SIZE;
		}
		sgpShip->mpComponent_Transform->mPosition.y = ShipY;
	}

	if (AEInputCheckCurr(VK_LEFT))
	{
		ShipX -= 5.0f;
		if (ShipX < (int)winMinX + SHIP_SIZE)
		{
			ShipX = winMinX + SHIP_SIZE;
		}
		sgpShip->mpComponent_Transform->mPosition.x = ShipX;
	}

	if (AEInputCheckCurr(VK_RIGHT))
	{
		ShipX += 5.0f;
		if (ShipX > (int)winMaxX - SHIP_SIZE)
		{
			ShipX = winMaxX - SHIP_SIZE;
		}
		sgpShip->mpComponent_Transform->mPosition.x = ShipX;
	}

	/////////////////////////////////////////////////////////////////////////////////////////////////
	/////////////////////////////////////////////////////////////////////////////////////////////////
	// TO DO 5:
	// -- Create a bullet instance when SPACE is triggered, using the "GameObjInstCreate" function
	/////////////////////////////////////////////////////////////////////////////////////////////////
	/////////////////////////////////////////////////////////////////////////////////////////////////
	if (AEInputCheckTriggered(VK_SPACE))
	{
		GameObjectInstance *bullet;
		if (PlayerCAmmo > 0) 
		{
			 bullet = GameObjectInstanceCreate(OBJECT_TYPE_PLAYER_BULLET);

			//Bullet position
			Vector2DSet(&bullet->mpComponent_Transform->mPosition, sgpShip->mpComponent_Transform->mPosition.x,
				sgpShip->mpComponent_Transform->mPosition.y);
			//Bullet angle
			bullet->mpComponent_Transform->mAngle = sgpShip->mpComponent_Transform->mAngle;

			//Bullet scale
			bullet->mpComponent_Transform->mScaleX = 10.0;
			bullet->mpComponent_Transform->mScaleY = 10.0;

			//Bullet velocity
			Vector2DSet(&bullet->mpComponent_Physics->mVelocity, cosf(sgpShip->mpComponent_Transform->mAngle) * BULLET_SPEED,
				sinf(sgpShip->mpComponent_Transform->mAngle) * BULLET_SPEED);

			PlayerCAmmo--;
			
			if (PlayerCAmmo == 0)
			{
				timerP = 0.0f;
			}
		}
	}
	
	if (timerP > 2.5f && PlayerCAmmo == 0)
	{
		PlayerCAmmo = 5;
	}
	if (timerB > 2.5f && BotCAmmo == 0)
	{
		BotCAmmo = 5;
	}
	
	if (AEInputCheckTriggered('B'))
	{
		if (BotCAmmo > 0)
		{
			GameObjectInstance *bullet = GameObjectInstanceCreate(OBJECT_TYPE_BOT_BULLET);

			//Bullet position
			Vector2DSet(&bullet->mpComponent_Transform->mPosition, sgpBot->mpComponent_Transform->mPosition.x,
				sgpBot->mpComponent_Transform->mPosition.y);
			//Bullet angle
			bullet->mpComponent_Transform->mAngle = sgpBot->mpComponent_Transform->mAngle;

			//Bullet scale
			bullet->mpComponent_Transform->mScaleX = 10.0;
			bullet->mpComponent_Transform->mScaleY = 10.0;

			//Bullet velocity
			Vector2DSet(&bullet->mpComponent_Physics->mVelocity, cosf(sgpBot->mpComponent_Transform->mAngle) * BULLET_SPEED,
				sinf(sgpBot->mpComponent_Transform->mAngle) * BULLET_SPEED);
			
			BotCAmmo--;
			
			if (BotCAmmo == 0)
			{
				timerB = 0.0f;
			}
		}
	}

	/////////////////////////////////////////////////////////////////////////////////////////////////
	/////////////////////////////////////////////////////////////////////////////////////////////////
	// TO DO 2:
	// Update the positions of all active game object instances
	// -- Positions are updated here (P1 = V1*t + P0)
	// -- If implemented correctly, you will be able to control the ship (basic 2D movement)
	/////////////////////////////////////////////////////////////////////////////////////////////////
	/////////////////////////////////////////////////////////////////////////////////////////////////
	for (i = 0; i < GAME_OBJ_INST_NUM_MAX; i++)
	{
		GameObjectInstance* pInst = sgGameObjectInstanceList + i;

		// skip non-active object
		if ((pInst->mFlag & FLAG_ACTIVE) == 0)
			continue;

		Vector2DScaleAdd(&pInst->mpComponent_Transform->mPosition, &pInst->mpComponent_Physics->mVelocity, &pInst->mpComponent_Transform->mPosition, (float)frameTime);
	}

	/////////////////////////////////////////////////////////////////////////////////////////////////
	/////////////////////////////////////////////////////////////////////////////////////////////////
	// TO DO 9: Check for collision
	// Important: Take the object instance's scale values into consideration when checking for collision.
	// -- Asteroid - Bullet: Rectangle to Point check. If they collide, destroy both.
	// -- Asteroid - Ship: Rectangle to Rectangle check. If they collide, destroy the asteroid, 
	//    reset the ship position to the center of the screen.
	// -- Asteroid - Homing Missile: Rectangle to Rectangle check.
	/////////////////////////////////////////////////////////////////////////////////////////////////
	/////////////////////////////////////////////////////////////////////////////////////////////////
	/*
	for each object instance: oi1
		if oi1 is not active
			skip

		if oi1 is an asteroid
			for each object instance oi2
				if(oi2 is not active or oi2 is an asteroid)
					skip

				if(oi2 is the ship)
					Check for collision between the ship and the asteroid
					Update game behavior accordingly
					Update "Object instances array"
				else
				if(oi2 is a bullet)
					Check for collision between the bullet and the asteroid
					Update game behavior accordingly
					Update "Object instances array"
				else
				if(oi2 is a missle)
					Check for collision between the missile and the asteroid
					Update game behavior accordingly
					Update "Object instances array"
	*/

	for (i = 0; i < GAME_OBJ_INST_NUM_MAX; i++)
	{
		GameObjectInstance* pInst = sgGameObjectInstanceList + i;

		if ((pInst->mFlag & FLAG_ACTIVE) == 0)
			continue;

		if ((pInst->mpComponent_Sprite->mpShape->mType == OBJECT_TYPE_PLAYER_BULLET) || (pInst->mpComponent_Sprite->mpShape->mType == OBJECT_TYPE_BOT_BULLET))
		{
			// destroy the bullet when it leaves the viewport
			if (pInst->mpComponent_Transform->mPosition.x < winMinX || pInst->mpComponent_Transform->mPosition.x > winMaxX
				|| pInst->mpComponent_Transform->mPosition.y < winMinY || pInst->mpComponent_Transform->mPosition.y > winMaxY) {
				GameObjectInstanceDestroy(pInst);
				continue;
			}
		}
	
		if (pInst->mpComponent_Sprite->mpShape->mType == OBJECT_TYPE_BOT) {

			int j;
			for (j = 0; j < GAME_OBJ_INST_NUM_MAX; j++) {

				GameObjectInstance* pjInst = sgGameObjectInstanceList + j;

				if ((pjInst->mFlag & FLAG_ACTIVE) == 0)
					continue;

			
				if (pjInst->mpComponent_Sprite->mpShape->mType == OBJECT_TYPE_PLAYER_BULLET) {
					if (pInst->mFlag != 0) {
						if (StaticPointToStaticRect(
							&pjInst->mpComponent_Transform->mPosition,
							&pInst->mpComponent_Transform->mPosition,
							pInst->mpComponent_Transform->mScaleX,
							pInst->mpComponent_Transform->mScaleY) == 1) {
							GameObjectInstanceDestroy(pjInst);
							sgBotLives = ((int)(BotCHP / 25) + 1) > 4 ? 4 : ((int)(BotCHP / 25) + 1);
							BotCHP -= 8.0f;
							if (BotCHP < 0.0f)
							{
								BotCHP = 100.0f;
								sgPlayerScore++;
								sgBotLives = 4;
								AESysPrintf("PlayerScore = %d | BotScore = %d\n", sgPlayerScore, sgBotScore);
							}
						}
					}
				}
			}

			{
				Vector2D DirectionVec, BotVec;
				Vector2DSub(&DirectionVec, &sgpShip->mpComponent_Transform->mPosition, &sgpBot->mpComponent_Transform->mPosition);
				Vector2DSet(&BotVec, cosf(sgpBot->mpComponent_Transform->mAngle), sinf(sgpBot->mpComponent_Transform->mAngle));
				toMoveX = FindPlayer(DirectionVec, BotVec);
			}
			
			toMoveY = getFuzzyOutputY(BotCHP, (float)BotCAmmo, fabs(sgpShip->mpComponent_Transform->mPosition.y));
			
			if (toMoveX > 3.5f)
			{
				sgpBot->mpComponent_Transform->mPosition.x += 3.5f;
				BotX = sgpBot->mpComponent_Transform->mPosition.x;
				toMoveX -= 3.5f;
			}
			else if(toMoveX < -3.5f)
			{
				sgpBot->mpComponent_Transform->mPosition.x -= 3.5f;
				BotX = sgpBot->mpComponent_Transform->mPosition.x;
				toMoveX -= 3.5f;
			}

			if ((toMoveY - BotY) > 1.75f)
			{
				sgpBot->mpComponent_Transform->mPosition.y += 3.5f;
				BotY = sgpBot->mpComponent_Transform->mPosition.y;
				toMoveY -= 3.5f;
			}
			else if ((toMoveY-BotY) < -1.75f)
			{
				sgpBot->mpComponent_Transform->mPosition.y -= 3.5f;
				BotY = sgpBot->mpComponent_Transform->mPosition.y;
				toMoveY -= 3.5f;
			}
			else
			{
				sgpBot->mpComponent_Transform->mPosition.y = toMoveY;
				BotY = sgpBot->mpComponent_Transform->mPosition.y;
			}
		}

		if (pInst->mpComponent_Sprite->mpShape->mType == OBJECT_TYPE_SHIP) {

			int j;
			for (j = 0; j < GAME_OBJ_INST_NUM_MAX; j++) {

				GameObjectInstance* pjInst = sgGameObjectInstanceList + j;

				if ((pjInst->mFlag & FLAG_ACTIVE) == 0)
					continue;


				if (pjInst->mpComponent_Sprite->mpShape->mType == OBJECT_TYPE_BOT_BULLET) {
					if (pInst->mFlag != 0) {
						if (StaticPointToStaticRect(
							&pjInst->mpComponent_Transform->mPosition,
							&pInst->mpComponent_Transform->mPosition,
							pInst->mpComponent_Transform->mScaleX,
							pInst->mpComponent_Transform->mScaleY) == 1) {
							
							GameObjectInstanceDestroy(pjInst);

							sgShipLives = ((int)(PlayerCHP / 25) + 1) > 4 ? 4 : ((int)(PlayerCHP / 25) + 1);
							PlayerCHP -= 8.0f;
							if (PlayerCHP < 0.0f)
							{
								PlayerCHP = 100.0f;
								sgBotScore++;
								sgShipLives = 4;
								AESysPrintf("PlayerScore = %d | BotScore = %d\n", sgPlayerScore, sgBotScore);
							}
						}
					}
				}
			}
		}
	}

	// =====================================
	// calculate the matrix for all objects
	// =====================================

	
	for (i = 0; i < GAME_OBJ_INST_NUM_MAX; i++)
	{
		Matrix2D		 trans, rot, scale, transform;
		GameObjectInstance* pInst = sgGameObjectInstanceList + i;
		
		// skip non-active object
		if ((pInst->mFlag & FLAG_ACTIVE) == 0)
			continue;

		/////////////////////////////////////////////////////////////////////////////////////////////////
		/////////////////////////////////////////////////////////////////////////////////////////////////
		// TO DO 1:
		// -- Build the transformation matrix of each active game object instance
		// -- After you implement this step, you should see the player's ship
		// -- Reminder: Scale should be applied first, then rotation, then translation.
		/////////////////////////////////////////////////////////////////////////////////////////////////
		/////////////////////////////////////////////////////////////////////////////////////////////////


		// Compute the scaling matrix
		// Compute the rotation matrix 
		// Compute the translation matrix
		// Concatenate the 3 matrix in the correct order in the object instance's transform component's "mTransform" matrix

		Matrix2DScale(&scale, pInst->mpComponent_Transform->mScaleX, pInst->mpComponent_Transform->mScaleY);

		Matrix2DTranslate(&trans, pInst->mpComponent_Transform->mPosition.x, pInst->mpComponent_Transform->mPosition.y);

		Matrix2DRotRad(&rot, pInst->mpComponent_Transform->mAngle);

		Matrix2DConcat(&transform, &rot, &scale);
		Matrix2DConcat(&pInst->mpComponent_Transform->mTransform, &trans, &transform);

	}

}

// ---------------------------------------------------------------------------

void GameStateAsteroidsDraw(void)
{
	int i;

	
		AEGfxSetRenderMode(AE_GFX_RM_COLOR);
		AEGfxTextureSet(NULL, 0, 0);
		AEGfxSetTintColor(1.0f, 1.0f, 1.0f, 1.0f);
		

		// draw all object instances in the list
		for (i = 0; i < GAME_OBJ_INST_NUM_MAX; i++)
		{
			GameObjectInstance* pInst = sgGameObjectInstanceList + i;

			// skip non-active object
			if ((pInst->mFlag & FLAG_ACTIVE) == 0)
				continue;

			// Already implemented. Explanation:
			// Step 1 & 2 are done outside the for loop (AEGfxSetRenderMode, AEGfxTextureSet, AEGfxSetTintColor) since all our objects share the same material.
			// If you want to have objects with difference materials (Some with textures, some without, some with transparency etc...)
			// then you'll need to move those functions calls inside the for loop
			// 1 - Set Render Mode (Color or texture)
			// 2 - Set all needed parameters (Color blend, textures, etc..)
			// 3 - Set the current object instance's mTransform matrix using "AEGfxSetTransform"
			// 4 - Draw the shape used by the current object instance using "AEGfxMeshDraw"

			AEGfxSetRenderMode(AE_GFX_RM_COLOR);
			AEGfxTextureSet(NULL, 0, 0);
			AEGfxSetTintColor(1.0f, 1.0f, 1.0f, 1.0f);

			AEGfxSetTransform(pInst->mpComponent_Transform->mTransform.m);
			AEGfxMeshDraw(pInst->mpComponent_Sprite->mpShape->mpMesh, AE_GFX_MDM_TRIANGLES);
		}

		//Player Lives
		for (i = 0; i < sgShipLives; i++) {
			AEGfxSetRenderMode(AE_GFX_RM_TEXTURE);
			AEGfxSetPosition(ShipX + (-22.0f + i * 15.0f), ShipY - 40.0f);
			AEGfxTextureSet(sgTextures[LIVES], 0.0f, 0.0f);
			AEGfxMeshDraw(Lives, AE_GFX_MDM_TRIANGLES);
		}

		//Bot Lives
		for (i = 0; i < sgBotLives; i++) {
			AEGfxSetRenderMode(AE_GFX_RM_TEXTURE);
			AEGfxSetPosition(BotX + (-22.0f + i * 15.0f), BotY + 40.0f);
			AEGfxTextureSet(sgTextures[LIVES], 0.0f, 0.0f);
			AEGfxMeshDraw(Lives, AE_GFX_MDM_TRIANGLES);
		}

		//Player Ammo
		for (i = 0; i < PlayerCAmmo; i++) {
			AEGfxSetRenderMode(AE_GFX_RM_TEXTURE);
			AEGfxSetPosition(ShipX + 40.0f, ShipY + (25.0f - i * 12.0f));
			AEGfxTextureSet(sgTextures[AMMO], 0.0f, 0.0f);
			AEGfxMeshDraw(Ammo, AE_GFX_MDM_TRIANGLES);
		}

		//BotAmmo
		for (i = 0; i < BotCAmmo; i++) {
			AEGfxSetRenderMode(AE_GFX_RM_TEXTURE);
			AEGfxSetPosition(BotX - 40.0f, BotY + (25.0f - i * 12.0f));
			AEGfxTextureSet(sgTextures[AMMO], 0.0f, 0.0f);
			AEGfxMeshDraw(Ammo, AE_GFX_MDM_TRIANGLES);
		}
}

// ---------------------------------------------------------------------------

void GameStateAsteroidsFree(void)
{
	/////////////////////////////////////////////////////////////////////////////////////////////////
	/////////////////////////////////////////////////////////////////////////////////////////////////
	// TO DO 12:
	//  -- Destroy all the active game object instances, using the “GameObjInstanceDestroy” function.
	//  -- Reset the number of active game objects instances
	/////////////////////////////////////////////////////////////////////////////////////////////////
	/////////////////////////////////////////////////////////////////////////////////////////////////
	int i;

	for (i = 0; i < GAME_OBJ_INST_NUM_MAX; i++)
	{
		GameObjectInstanceDestroy(sgGameObjectInstanceList + i);
	}
	// zero the game object instance array
	memset(sgGameObjectInstanceList, 0, sizeof(GameObjectInstance)* GAME_OBJ_INST_NUM_MAX);
	// No game object instances (sprites) at this point
	sgGameObjectInstanceNum = 0;

}

// ---------------------------------------------------------------------------

void GameStateAsteroidsUnload(void)
{
	/////////////////////////////////////////////////////////////////////////////////////////////////
	/////////////////////////////////////////////////////////////////////////////////////////////////
	// TO DO 13:
	//  -- Destroy all the shapes, using the “AEGfxMeshFree” function.
	/////////////////////////////////////////////////////////////////////////////////////////////////
	/////////////////////////////////////////////////////////////////////////////////////////////////
	int i;
	for (i = 0; i < SHAPE_NUM_MAX; i++) {
		AEGfxMeshFree(sgShapes[i].mpMesh);
	}



	// Zero the shapes array
	memset(sgShapes, 0, sizeof(Shape) * SHAPE_NUM_MAX);
	// No shapes at this point
	sgShapeNum = 0;

	
}

// ---------------------------------------------------------------------------

GameObjectInstance* GameObjectInstanceCreate(unsigned int ObjectType)			// From OBJECT_TYPE enum)
{
	unsigned long i;
	
	// loop through the object instance list to find a non-used object instance
	for (i = 0; i < GAME_OBJ_INST_NUM_MAX; i++)
	{
		GameObjectInstance* pInst = sgGameObjectInstanceList + i;

		// Check if current instance is not used
		if (pInst->mFlag == 0)
		{
			// It is not used => use it to create the new instance

			// Active the game object instance
			pInst->mFlag = FLAG_ACTIVE;

			pInst->mpComponent_Transform = 0;
			pInst->mpComponent_Sprite = 0;
			pInst->mpComponent_Physics = 0;
			pInst->mpComponent_Target = 0;

			// Add the components, based on the object type
			switch (ObjectType)
			{
			case OBJECT_TYPE_SHIP:
				AddComponent_Sprite(pInst, OBJECT_TYPE_SHIP);
				AddComponent_Transform(pInst, 0, 0.0f, 1.0f, 1.0f);
				AddComponent_Physics(pInst, 0);
				break;

			case OBJECT_TYPE_PLAYER_BULLET:
				AddComponent_Sprite(pInst, OBJECT_TYPE_PLAYER_BULLET);
				AddComponent_Transform(pInst, 0, 0.0f, 1.0f, 1.0f);
				AddComponent_Physics(pInst, 0);
				break;

			case OBJECT_TYPE_BOT_BULLET:
				AddComponent_Sprite(pInst, OBJECT_TYPE_BOT_BULLET);
				AddComponent_Transform(pInst, 0, 0.0f, 1.0f, 1.0f);
				AddComponent_Physics(pInst, 0);
				break;

			case OBJECT_TYPE_BOT:
				AddComponent_Sprite(pInst, OBJECT_TYPE_BOT);
				AddComponent_Transform(pInst, 0, 0.0f, 1.0f, 1.0f);
				AddComponent_Physics(pInst, 0);
				break;
			}

			++sgGameObjectInstanceNum;

			// return the newly created instance
			return pInst;
		}
	}

	// Cannot find empty slot => return 0
	return 0;
}

// ---------------------------------------------------------------------------

void GameObjectInstanceDestroy(GameObjectInstance* pInst)
{
	// if instance is destroyed before, just return
	if (pInst->mFlag == 0)
		return;

	// Zero out the mFlag
	pInst->mFlag = 0;

	RemoveComponent_Transform(pInst);
	RemoveComponent_Sprite(pInst);
	RemoveComponent_Physics(pInst);
	RemoveComponent_Target(pInst);

	--sgGameObjectInstanceNum;
}

// ---------------------------------------------------------------------------

void AddComponent_Transform(GameObjectInstance *pInst, Vector2D *pPosition, float Angle, float ScaleX, float ScaleY)
{
	if (0 != pInst)
	{
		if (0 == pInst->mpComponent_Transform)
		{
			pInst->mpComponent_Transform = (Component_Transform *)calloc(1, sizeof(Component_Transform));
		}

		Vector2D zeroVec2;
		Vector2DZero(&zeroVec2);

		pInst->mpComponent_Transform->mScaleX = ScaleX;
		pInst->mpComponent_Transform->mScaleY = ScaleY;
		pInst->mpComponent_Transform->mPosition = pPosition ? *pPosition : zeroVec2;;
		pInst->mpComponent_Transform->mAngle = Angle;
		pInst->mpComponent_Transform->mpOwner = pInst;
	}
}

// ---------------------------------------------------------------------------

void AddComponent_Sprite(GameObjectInstance *pInst, unsigned int ShapeType)
{
	if (0 != pInst)
	{
		if (0 == pInst->mpComponent_Sprite)
		{
			pInst->mpComponent_Sprite = (Component_Sprite *)calloc(1, sizeof(Component_Sprite));
		}
	
		pInst->mpComponent_Sprite->mpShape = sgShapes + ShapeType;
		pInst->mpComponent_Sprite->mpOwner = pInst;
	}
}

// ---------------------------------------------------------------------------

void AddComponent_Physics(GameObjectInstance *pInst, Vector2D *pVelocity)
{
	if (0 != pInst)
	{
		if (0 == pInst->mpComponent_Physics)
		{
			pInst->mpComponent_Physics = (Component_Physics *)calloc(1, sizeof(Component_Physics));
		}

		Vector2D zeroVec2;
		Vector2DZero(&zeroVec2);

		pInst->mpComponent_Physics->mVelocity = pVelocity ? *pVelocity : zeroVec2;
		pInst->mpComponent_Physics->mpOwner = pInst;
	}
}

// ---------------------------------------------------------------------------

void AddComponent_Target(GameObjectInstance *pInst, GameObjectInstance *pTarget)
{
	if (0 != pInst)
	{
		if (0 == pInst->mpComponent_Target)
		{
			pInst->mpComponent_Target = (Component_Target *)calloc(1, sizeof(Component_Target));
		}

		pInst->mpComponent_Target->mpTarget = pTarget;
		pInst->mpComponent_Target->mpOwner = pInst;
	}
}

// ---------------------------------------------------------------------------

void RemoveComponent_Transform(GameObjectInstance *pInst)
{
	if (0 != pInst)
	{
		if (0 != pInst->mpComponent_Transform)
		{
			free(pInst->mpComponent_Transform);
			pInst->mpComponent_Transform = 0;
		}
	}
}

// ---------------------------------------------------------------------------

void RemoveComponent_Sprite(GameObjectInstance *pInst)
{
	if (0 != pInst)
	{
		if (0 != pInst->mpComponent_Sprite)
		{
			free(pInst->mpComponent_Sprite);
			pInst->mpComponent_Sprite = 0;
		}
	}
}

// ---------------------------------------------------------------------------

void RemoveComponent_Physics(GameObjectInstance *pInst)
{
	if (0 != pInst)
	{
		if (0 != pInst->mpComponent_Physics)
		{
			free(pInst->mpComponent_Physics);
			pInst->mpComponent_Physics = 0;
		}
	}
}

// ---------------------------------------------------------------------------

void RemoveComponent_Target(GameObjectInstance *pInst)
{
	if (0 != pInst)
	{
		if (0 != pInst->mpComponent_Target)
		{
			free(pInst->mpComponent_Target);
			pInst->mpComponent_Target = 0;
		}
	}
}

float FindPlayer(Vector2D DirVec, Vector2D BotVec)
{
	float dotProduct, angle, distance;
	Vector2D UpVec;

	dotProduct = Vector2DDotProduct(&DirVec, &BotVec);
	Vector2DSet(&UpVec, -DirVec.y, DirVec.x);


	angle = acosf(dotProduct / (Vector2DLength(&DirVec)*Vector2DLength(&BotVec)));

	if (Vector2DDotProduct(&UpVec, &BotVec) > 0)
	{
		angle = -angle;//move left;
	}

	distance = sin(angle) * Vector2DLength(&DirVec);
	//AESysPrintf("Angle %f toMove %f\n", angle * 180 / PI, distance);
	return distance;
}

float Triangle(float a, float b, float c, float x, int *side)
{


	if (x >= a && x <= b) {
		*side = 1;
		if (a == b)
		{
			return 1.0f;
		}
		else
		{
			
			return ((x - a) / (b - a));
		}
	}
	else if (x > b && x <= c)
	{
		*side = 2;
		if (c == b)
		{
			return 1.0f;
		}
		else
		{
			return ((c - x) / (c - b));
		}
	}
	else
	{
		return 0.0f;
	}
}

float getFuzzyOutputY(float x, float y, float z)
{
	float outy = BotY;
	int i = 0, h, a, p, o;
	int side;
	float hh, aa, pp, numerator, denominator;
	float hmid, amid, pmid, bmid;

	float HP[3][3] = {
		{0.0f,0.0f,50.0f},
		{25.0f,50.0f,75.0f},
		{50.0f,100.0f,100.0f}
	};
	float AM[2][3] = {
		{0.0f,0.0f,3.5f},
		{1.5f,5.0f,5.0f}
	};
	float P[3][3] = {
		{0.0f,0.0f,200.0f},
		{100.0f,175.0f,250.0f},
		{150.0f,300.0f,300.0f}
	};
	float BY[3][3] = {
		{0.0f,0.0f,150.0f},			//OF - 11,14,15,17
		{100.0f,175.0f,250.0f},		//NO - 1,2,4,5,8,9,10,12,13,16
		{150.0f,300.0f,300.0f}		//DF - 0,3,6,7
	};
	float firings[18], w[18];
	float alpha, beta, gamma, constant;
	
	for (h = 0; h < 3; ++h)
	{
		hh = Triangle(HP[h][0], HP[h][1], HP[h][2], x , &side);
		for (a = 0; a < 2; ++a)
		{
			aa = Triangle(AM[a][0], AM[a][1], AM[a][2], y, &side);
			for (p = 0; p < 3; ++p)
			{
				pp = Triangle(P[p][0], P[p][1], P[p][2], z, &side);
				if (i == 0 || i == 3 || i == 6 || i == 7)
				{
					o = 2;
				}
				else if (i == 11 || i == 14 || i == 15 || i == 17)
				{
					o = 0;
				}
				else
				{
					o = 1;
				}
				alpha = (BY[o][2] - BY[o][0]) / (HP[h][2] - HP[h][0]);
				beta = (BY[o][2] - BY[o][0]) / (AM[a][2] - AM[a][0]);
				gamma = (BY[o][2] - BY[o][0]) / (P[p][2] - P[p][0]);
				constant = BY[o][1] - (alpha*HP[h][1]) - (beta*AM[a][1]) - (gamma*P[p][1]);
				firings[i] = min(hh, min(aa, pp));
				w[i] = alpha * x + beta * y + gamma * z + constant;
				++i;
			}
		}
	}
	

	numerator = denominator = 0.0f;

	for (i = 0; i < 18; ++i)
	{
		numerator += firings[i] * w[i];
		denominator += firings[i];
	}
	
	outy = numerator / denominator;
	if (outy < SHIP_SIZE)
	{
		outy = SHIP_SIZE;
	}
	else if (outy > (300.0f - SHIP_SIZE))
	{
		outy = 300.0f - SHIP_SIZE;
	}
	return outy;
}