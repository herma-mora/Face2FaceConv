#include "Console.h"
#include "Settings.h"
#include "skse64/GameAPI.h"
#include "skse64/ObScript.h"
#include "skse64_common/SafeWrite.h"

static ObScriptCommand * s_hijackedCommand = nullptr;

static bool Cmd_SetF2FVariable_Execute(void * paramInfo, void * scriptData, TESObjectREFR * thisObj, void * containingObj, void * scriptObj, void * locals, double * result, void * opcodeOffsetPtr)
{
	ObScriptCommand::ScriptData * scriptDataEx = (ObScriptCommand::ScriptData*)scriptData;
	ObScriptCommand::StringChunk *strChunk = (ObScriptCommand::StringChunk*)scriptDataEx->GetChunk();
	std::string name = strChunk->GetString();

	ObScriptCommand::IntegerChunk * intChunk = (ObScriptCommand::IntegerChunk*)strChunk->GetNext();
	int val = intChunk->GetInteger();

	if (Settings::Set(name.c_str(), val))
		Console_Print("> set Settings::%s = %d", name.c_str(), val);
	else
		Console_Print("> (Error) Settings::%s is not found.", name.c_str(), val);

	return true;
}

namespace ConsoleCommand
{
	void Register()
	{
		_MESSAGE("ConsoleCommand::Register()");

		for (ObScriptCommand * iter = g_firstConsoleCommand; iter->opcode < kObScript_NumConsoleCommands + kObScript_ConsoleOpBase; ++iter)
		{
			if (!strcmp(iter->longName, "TogglePathLine")) // unused
			{
				s_hijackedCommand = iter;
				break;
			}
		}

		ObScriptCommand cmd = *s_hijackedCommand;
		static ObScriptParam params[] = {
			{ "String", 0, 0 },
			{ "Integer", 1, 0 }
		};
		cmd.longName = "SetF2FVariable";
		cmd.shortName = "sf2fv";
		cmd.helpText = "";
		cmd.needsParent = 0;
		cmd.numParams = 2;
		cmd.params = params;
		cmd.execute = &Cmd_SetF2FVariable_Execute;
		cmd.eval = nullptr;
		cmd.flags = 0;

		SafeWriteBuf((uintptr_t)s_hijackedCommand, &cmd, sizeof(cmd));
	}
}
