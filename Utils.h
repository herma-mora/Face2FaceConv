#pragma once

struct AngleZX
{
	float  z;
	float  x;
	float  distance;
};


class TESCameraController
{
public:
	TESCameraController() {}

	UInt32 unk00;
	float  startRotZ; // 04
	float  startRotX; // 08
	float  endRotZ;   // 0C
	float  endRotX;   // 10
	float  unk14;     // 14
	float  unk18;     // 18
	UInt8  unk1C;     // 1C
	UInt8  pad1D[3];  // 1D

	static TESCameraController* GetSingleton();
};