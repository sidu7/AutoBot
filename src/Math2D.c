/* Start Header -------------------------------------------------------

Copyright (C) 2018 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the prior 
written consent of DigiPen Institute 
of Technology is prohibited.
File Name:		Math2D.c 
Purpose:		Math Functions to check for collision between point, Circle and Rectangle
Language:		C
Platform:		Visual Studio 2017 | Visual C++ 14.1 | Windows 10 Home                         
Project:		CS529_sidhantt_1
Author:			Sidhant Tumma | sidhant.t | 60002218
Creation date:	09/18/2018

- End Header --------------------------------------------------------*/

#include "Math2D.h"
#include "stdio.h"


/*
This function checks if the point P is colliding with the circle whose
center is "Center" and radius is "Radius"
*/
int StaticPointToStaticCircle(Vector2D *pP, Vector2D *pCenter, float Radius)
{
	
	if (Vector2DSquareDistance(pP, pCenter) > Radius*Radius) {
		return 0;
	}
	return 1;
}


/*
This function checks if the point Pos is colliding with the rectangle
whose center is Rect, width is "Width" and height is Height
*/
int StaticPointToStaticRect(Vector2D *pPos, Vector2D *pRect, float Width, float Height)
{
	float hw = Width / 2;
	float hh = Height / 2;

	float left = pRect->x - hw;
	float right = pRect->x + hw;
	float top = pRect->y + hh;
	float bottom = pRect->y - hh;

	if (pPos->x < left || pPos->x > right || pPos->y > top || pPos->y < bottom) {
		return 0;
	}

	return 1;
}

/*
This function checks for collision between 2 circles.
Circle0: Center is Center0, radius is "Radius0"
Circle1: Center is Center1, radius is "Radius1"
*/
int StaticCircleToStaticCircle(Vector2D *pCenter0, float Radius0, Vector2D *pCenter1, float Radius1)
{
	float TotalRadius = Radius0 + Radius1;
	if (Vector2DSquareDistance(pCenter0, pCenter1) > TotalRadius*TotalRadius) {
		return 0;
	}
	return 1;
}

/*
This functions checks if 2 rectangles are colliding
Rectangle0: Center is pRect0, width is "Width0" and height is "Height0"
Rectangle1: Center is pRect1, width is "Width1" and height is "Height1"
*/
int StaticRectToStaticRect(Vector2D *pRect0, float Width0, float Height0, Vector2D *pRect1, float Width1, float Height1)
{
	float hw0, hh0, hw1, hh1;
	float left0, right0, top0, bottom0;
	float left1, right1, top1, bottom1;

	hw0 = Width0 / 2;
	hh0 = Height0 / 2;

	left0 = pRect0->x - hw0;
	right0 = pRect0->x + hw0;
	top0 = pRect0->y + hh0;
	bottom0 = pRect0->y - hh0;

	hw1 = Width1 / 2;
	hh1 = Height1 / 2;

	left1 = pRect1->x - hw1;
	right1 = pRect1->x + hw1;
	top1 = pRect1->y + hh1;
	bottom1 = pRect1->y - hh1;


	if (right0 < left1 || left0 > right1 || top0 < bottom1 || bottom0 >top1) {
		return 0;
	}
  return 1;
}
