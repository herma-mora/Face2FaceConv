#include "Settings.h"
#include "skse64/GameStreams.h"
#include <string>

bool	Settings::bLockOn = true;
bool	Settings::bForceFirstPerson = true;
float	Settings::fCameraSpeed = 700;
float	Settings::f1stFOV = 65;
float	Settings::f2ndFOV = 60;
float	Settings::f3rdFOV = 55;
float	Settings::f4thFOV = 50;
float	Settings::f5thFOV = 40;
float	Settings::f6thFOV = 30;

bool Settings::SetBool(const char *name, bool val)
{
	if (_stricmp(name, "bLockOn") == 0)
	{
		bLockOn = val;
	}
	else if (_stricmp(name, "bForceFirstPerson") == 0)
	{
		bForceFirstPerson = val;
	}
	else
	{
		return false;
	}

	_MESSAGE("  %s = %d", name, val);

	return true;
}


bool Settings::SetFloat(const char *name, float val)
{
	if (_stricmp(name, "fCameraSpeed") == 0)
	{
		if (val < 100)
			val = 100;

		fCameraSpeed = val;
	}
	else if (_stricmp(name, "f1stFOV") == 0)
	{
		f1stFOV = val;
	}
	else if (_stricmp(name, "f2ndFOV") == 0)
	{
		f2ndFOV = val;
	}
	else if (_stricmp(name, "f3rdFOV") == 0)
	{
		f3rdFOV = val;
	}
	else if (_stricmp(name, "f4thFOV") == 0)
	{
		f4thFOV = val;
	}
	else if (_stricmp(name, "f5thFOV") == 0)
	{
		f5thFOV = val;
	}
	else if (_stricmp(name, "f6thFOV") == 0)
	{
		f6thFOV = val;
	}
	else
	{
		return false;
	}

	_MESSAGE("  %s = %d", name, (int)val);

	return true;
}


bool Settings::Set(const char *name, int val)
{
	if (!name || !name[0])
		return false;

	if (name[0] == 'b')
		return SetBool(name, (val != 0));
	if (name[0] == 'f')
		return SetFloat(name, (float)val);

	return false;
}


void Settings::Eval(const char *line)
{
	char buf[256];
	strcpy_s(buf, line);

	char *ctx;
	char *name = strtok_s(buf, "=", &ctx);
	if (name && name[0])
	{
		char *val = strtok_s(nullptr, "", &ctx);
		if (val && val[0])
		{
			if (val[0] == '-')
			{
				Set(name, -atoi(val + 1));
			}
			else
			{
				Set(name, atoi(val));
			}
		}
	}
}


void Settings::Load()
{
	BSResourceNiBinaryStream iniStream("SKSE\\Plugins\\Face2FaceConv.ini");
	if (!iniStream.IsValid())
		return;

	char buf[256];

	_MESSAGE("Settings::Load()");
	while (true)
	{
		UInt32 len = iniStream.ReadLine(buf, 250, '\n');
		if (len == 0)
			break;
		buf[len] = 0;

		Eval(buf);
	}
}
