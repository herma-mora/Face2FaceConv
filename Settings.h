#pragma once

struct Settings
{
	static bool		bLockOn;
	static bool		bForceFirstPerson;
	static float	fCameraSpeed;
	static float	f1stFOV;
	static float	f2ndFOV;
	static float	f3rdFOV;
	static float	f4thFOV;
	static float	f5thFOV;
	static float	f6thFOV;

	static bool SetBool(const char *name, bool val);
	static bool SetFloat(const char *name, float val);
	static bool Set(const char *name, int val);
	static void Eval(const char *line);

	static void Load();
};
