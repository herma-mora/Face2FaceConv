#include "skse64/PluginAPI.h"
#include "skse64_common/Relocation.h"
#include "skse64_common/BranchTrampoline.h"
#include "skse64_common/skse_version.h"
#include "skse64/GameCamera.h"
#include "skse64/GameData.h"
#include "skse64/GameMenus.h"
#include "skse64/GameEvents.h"
#include "skse64/NiNodes.h"
#include "xbyak/xbyak.h"
#include "Utils.h"
#include "Settings.h"
#include "Console.h"
#include <shlobj.h>

IDebugLog				gLog;
PluginHandle			g_pluginHandle = kPluginHandle_Invalid;
SKSEMessagingInterface	*	g_messaging = NULL;

void * g_moduleHandle = nullptr;

const float PI = 3.1415927;

int cameraStatesID;
bool afterThis = false;
//float curRotZ = 0;

float g_defaultWorldFOV;
float g_defaultFirstFOV;
float g_worldfovTo;
float g_firstfovTo;
int   g_fovStep;

bool bZoom = false;

TESObjectREFR* refTarget = NULL;
UInt32 cameraStateIDStarter;

RelocPtr<uintptr_t> _fDefaultWorldFOV(0x01E2CF00);
float &fDefaultWorldFOV = *(float*)_fDefaultWorldFOV.GetUIntPtr();

RelocAddr <uintptr_t> kOnCameraMoveAddr(0x0069EE5A);
RelocAddr <TESCameraController*> kGetSingletonAddr(0x01E16A10);

typedef TESObjectWEAP * GetEquippedWeapon_t(PlayerCharacter * player, bool isLeftHand);
RelocAddr<GetEquippedWeapon_t*> GetEquippedWeaponFunc(0x00626360);

typedef void SetAngleZ_t(PlayerCharacter * player, float angle);
RelocAddr<SetAngleZ_t*> SetAngleZ(0x005D1470);

typedef void SetAngleX_t(PlayerCharacter * player, float angle);
RelocAddr<SetAngleX_t*> SetAngleX(0x005EE200);

RelocAddr<uintptr_t> kDisablePlayerLookAddr(0x00705530);

TESCameraController* TESCameraController::GetSingleton()
{
	return (TESCameraController*)kGetSingletonAddr;
}

bool IsCameraFirstPerson()
{
	PlayerCamera* camera = PlayerCamera::GetSingleton();
	if (!camera)
		return false;

	return camera->cameraState == camera->cameraStates[camera->kCameraState_FirstPerson];
}


bool IsCameraThirdPerson()
{
	PlayerCamera* camera = PlayerCamera::GetSingleton();
	if (!camera)
		return false;

	return camera->cameraState == camera->cameraStates[camera->kCameraState_ThirdPerson2];
}

float GetDistance(TESObjectREFR * refTarget)
{
	float x1, y1, z1, x2, y2, z2, x, y, z;
	PlayerCharacter * player = (*g_thePlayer);
	x1 = player->pos.x;
	y1 = player->pos.y;
	z1 = player->pos.z;
	x2 = refTarget->pos.x;
	y2 = refTarget->pos.y;
	z2 = refTarget->pos.z;
	x = x2 - x1;
	y = y2 - y1;
	z = z2 - z1;
	return sqrt(x*x + y*y + z*z);
}

class DialogueMenuEventHandler : public BSTEventSink <MenuOpenCloseEvent>
{
public:
	virtual EventResult ReceiveEvent(MenuOpenCloseEvent * evn, EventDispatcher<MenuOpenCloseEvent> * dispatcher)
	{
		if (evn->menuName == (UIStringHolder::GetSingleton()->dialogueMenu))
		{
			if (MenuControls::GetSingleton()->beastForm)
				return kEvent_Continue;

			if (CALL_MEMBER_FN((*g_thePlayer), IsOnMount)())
				return kEvent_Continue;

			UInt32 cameraStateID;
			PlayerCamera * camera = PlayerCamera::GetSingleton();
			cameraStateID = camera->cameraState->stateId;

			if ((((*g_thePlayer)->actorState.flags04 >> 0x0E) & 0x0F) != 0)
				return kEvent_Continue;

			//FirstPersonState* fps = (FirstPersonState*)camera->cameraStates[PlayerCamera::kCameraState_FirstPerson];
			//curRotZ = (*g_thePlayer)->GetActorRotationZ(0) + fps->sittingRot;

			if (cameraStateID != 0 && cameraStateID != 9)
				return kEvent_Continue;

			if (evn->opening){
				afterThis = false;
				float distance = 0;
				float thisFOV = 0;
				TESObjectREFR* target = NULL;
				bZoom =	false;

				MenuTopicManager * mm = MenuTopicManager::GetSingleton();
				if (mm){
					target = mm->GetDialogueTarget(); // 68
				}
				if (!target)
				{
					UInt32 handle = 0;
					handle = mm->speakerHandle; // 6C
					TESObjectREFR * ref = NULL;
					if (handle != *g_invalidRefHandle)
						LookupREFRByHandle(&handle, &ref);

					if (ref)
						target = ref;
/*					
					if (CrosshairRefHandleHolder::GetSingleton())
					{
						UInt32 handle = 0;
						handle = CrosshairRefHandleHolder::GetSingleton()->CrosshairRefHandle();

						TESObjectREFR* ref = NULL;
						if (handle != *g_invalidRefHandle)
							LookupREFRByHandle(&handle, &ref);

						if (ref)
							target = ref;
					}
*/
				}
				if (target && target->formType == kFormType_Character)
				{
					bZoom = true;
					if (cameraStateID == PlayerCamera::kCameraState_ThirdPerson2){
						cameraStateIDStarter = PlayerCamera::kCameraState_ThirdPerson2;

						if (Settings::bForceFirstPerson)
							CALL_MEMBER_FN(camera, ForceFirstPerson)();
						else
						{
							if ((*g_thePlayer)->actorState.IsWeaponDrawn())
							{
								ThirdPersonState* tps = (ThirdPersonState*)camera->cameraStates[PlayerCamera::kCameraState_ThirdPerson2];
								tps->UpdateMode(false);
							}
						}
					}
					else{
						cameraStateIDStarter = PlayerCamera::kCameraState_FirstPerson;
					}

					refTarget = target;

					if (IsCameraFirstPerson())
					{
						distance = GetDistance(refTarget);

						if (distance < 50){
							thisFOV = Settings::f1stFOV;
						}
						else if (distance >= 50 && distance <= 75){
							thisFOV = Settings::f2ndFOV;
						}
						else if (distance > 75 && distance <= 100){
							thisFOV = Settings::f3rdFOV;
						}
						else if (distance > 100 && distance <= 125){
							thisFOV = Settings::f4thFOV;
						}
						else if (distance > 125 && distance <= 150){
							thisFOV = Settings::f5thFOV;
						}
						else{
							thisFOV = Settings::f6thFOV;
						}
					}
					else
					{
						thisFOV = Settings::f6thFOV;
					}

					int step = Settings::fCameraSpeed * 60 / 1000;
					if (step <= 0) step = 1;
					g_worldfovTo = thisFOV;
					g_firstfovTo = thisFOV;
					g_fovStep = step;
				}
			}
			else if (!evn->opening){
				refTarget = NULL;
				afterThis = false;

				if (bZoom)
				{
					if (cameraStateID <= PlayerCamera::kCameraState_ThirdPerson2 && cameraStateIDStarter == PlayerCamera::kCameraState_ThirdPerson2)
					{
						ThirdPersonState* tps = (ThirdPersonState*)camera->cameraStates[PlayerCamera::kCameraState_ThirdPerson2];
						if (cameraStateID == PlayerCamera::kCameraState_ThirdPerson2 && (*g_thePlayer)->actorState.IsWeaponDrawn())
						{
							tps->UpdateMode(true);
						}

						if (cameraStateID < PlayerCamera::kCameraState_ThirdPerson2)
						{
							tps->basePosX = tps->fOverShoulderPosX;
							tps->basePosY = tps->fOverShoulderCombatAddY;
							tps->basePosZ = tps->fOverShoulderPosZ;
							tps->curPosY = tps->dstPosY;

							CALL_MEMBER_FN(camera, SetCameraState)(tps);
						}
					}

					int step = Settings::fCameraSpeed * 60 / 1000;
					if (step <= 0) step = 1;
					g_worldfovTo = fDefaultWorldFOV;
					g_firstfovTo = fDefaultWorldFOV;
					g_fovStep = step;
				}
			}
		}
		else if (evn->menuName == (UIStringHolder::GetSingleton()->raceSexMenu)) // fix for face sculptor
		{
			if (evn->opening)
			{
				PlayerCamera * camera = PlayerCamera::GetSingleton();
				if (camera)
				{
					camera->worldFOV = fDefaultWorldFOV;
					camera->firstPersonFOV = fDefaultWorldFOV;
				}
			}
		}
		return kEvent_Continue;
	}
};

DialogueMenuEventHandler			g_dialogueMenuEventHandler;

void MessageHandler(SKSEMessagingInterface::Message * msg)
{
	switch (msg->type)
	{
	case SKSEMessagingInterface::kMessage_InputLoaded:
	{
		Settings::Load();

		MenuManager *mm = MenuManager::GetSingleton();
		if (mm){
			mm->MenuOpenCloseEventDispatcher()->AddEventSink(&g_dialogueMenuEventHandler);
		}
	}
		break;
	}
}

static bool CheckDistance(TESObjectREFR * target, NiPoint3 * xyz)
{
	if (target && target->formType != kFormType_Character)
		return false;

	if (target->GetNiNode() == NULL)
		return false;

	TESCameraController::GetSingleton()->unk1C = 1;

	float targetPosX, targetPosY, targetPosZ, cameraPosX, cameraPosY, cameraPosZ;
	Actor * actor = (Actor*)target;

	TESRace* race = actor->race;
	BGSBodyPartData* bodyPart = race->bodyPartData;
	BGSBodyPartData::Data* data;

	data = bodyPart->part[0];
	if (data)
	{
		NiAVObject* object = actor->GetNiNode();
		if (object)
		{
			const char * necknameData;

			if (bodyPart->formID == 0x13492 || bodyPart->formID == 0x401BC4A){
				necknameData = "NPC NeckHub";
			}
			else if (bodyPart->formID == 0xBA549){
				necknameData = "Mcrab_Body";
			}
			else if (bodyPart->formID == 0x4FBF5) {
				necknameData = "Canine_Neck2";
			}
			else if (bodyPart->formID == 0x60716){
				necknameData = "HorseNeck4";
			}
			else if (bodyPart->formID == 0x20E26){
				necknameData = "Sabrecat_Neck[Nek2]";
			}
			else if (bodyPart->formID == 0x76B30){
				necknameData = "ElkNeck4";
			}
			else if (bodyPart->formID == 0x868FC){
				necknameData = "NPC Neck2";
			}
			else if (bodyPart->formID == 0x6DC9C){
				necknameData = "RabbitNeck2";
			}
			else if (bodyPart->formID == 0xA919E){
				necknameData = "Neck2";
			}
			else if (bodyPart->formID == 0x8691C){
				necknameData = "FireAtronach_Neck [Neck]";
			}
			else if (bodyPart->formID == 0x6B7C9){
				necknameData = "Jaw";
			}
			else if (bodyPart->formID == 0x517AB || bodyPart->formID == 0x4028537){
				necknameData = "NPC Neck [Neck]2";
			}
			else if (bodyPart->formID == 0x59060){
				necknameData = "[Neck3]";
			}
			else if (bodyPart->formID == 0x2005205){
				necknameData = "ChaurusFlyerNeck";
			}
			else if (bodyPart->formID == 0x4E782){
				necknameData = "Neck3";
			}
			else if (bodyPart->formID == 0x43592){
				necknameData = "DragPriestNPC Neck [Neck]";
			}
			else if (bodyPart->formID == 0x5DDA2 || bodyPart->formID == 0x4017F53){
				necknameData = "NPC Neck";
			}
			else if (bodyPart->formID == 0x17929){
				necknameData = "[body]";
			}
			else if (bodyPart->formID == 0x6F276){
				necknameData = "Goat_Neck4";
			}
			else if (bodyPart->formID == 0x8CA6B){
				necknameData = "Horker_Neck4";
			}
			else if (bodyPart->formID == 0x538F9){
				necknameData = "IW Seg01";
			}
			else if (bodyPart->formID == 0x59255){
				necknameData = "Mammoth Neck";
			}
			else if (bodyPart->formID == 0x264EF){
				necknameData = "Neck";
			}
			else if (bodyPart->formID == 0x42529){
				necknameData = "Wisp Neck";
			}
			else if (bodyPart->formID == 0x86F43){
				necknameData = "Witchlight Body Lag";
			}
			else if (bodyPart->formID == 0x40C6A){
				necknameData = "SlaughterfishNeck";
			}
			else if (bodyPart->formID == 0x4019AD2){
				necknameData = "Neck [Neck]";
			}
			else if (bodyPart->formID == 0x401FEB9){
				necknameData = "NetchPelvis [Pelv]";
			}
			else if (bodyPart->formID == 0x401E2A3){
				necknameData = "Boar_Reikling_Neck";
			}
			else if (bodyPart->formID == 0x401DCBC){
				necknameData = "NPC COM [COM ]";
			}
			else if (bodyPart->formID == 0x402B018){
				necknameData = "MainBody";
			}
			else if (bodyPart->formID == 0x7874D){
				necknameData = "NPC Spine2";
			}
			else if (bodyPart->formID == 0x81C7A){
				necknameData = "DwarvenSpiderBody";
			}
			else if (bodyPart->formID == 0x800EC){
				necknameData = "NPC LowerJaw";
			}
			else{
				necknameData = "NPC Neck [Neck]";
			}

			BSFixedString neckName(necknameData);

			object = object->GetObjectByName(&neckName.data);
			if (object)
			{
				targetPosX = object->m_worldTransform.pos.x;
				targetPosY = object->m_worldTransform.pos.y;
				targetPosZ = object->m_worldTransform.pos.z;
			}
		}
	}
	else
	{
		NiPoint3 pos;
		actor->GetMarkerPosition(&pos);
		targetPosX = pos.x;
		targetPosY = pos.y;
		targetPosZ = pos.z;
	}

	PlayerCharacter* player = *g_thePlayer;
	PlayerCamera * pcCamera = PlayerCamera::GetSingleton();
	FirstPersonState* fps = (FirstPersonState*)pcCamera->cameraStates[pcCamera->kCameraState_FirstPerson];
	ThirdPersonState* tps = (ThirdPersonState*)pcCamera->cameraStates[pcCamera->kCameraState_ThirdPerson2];
	if (pcCamera->cameraState == fps || pcCamera->cameraState == tps)
	{
		NiNode * node = pcCamera->cameraNode;
		if (node)
		{
			cameraPosX = node->m_worldTransform.pos.x;
			cameraPosY = node->m_worldTransform.pos.y;
			cameraPosZ = node->m_worldTransform.pos.z;
		}
	}
	else
	{
		NiPoint3 playerPos;

		player->GetMarkerPosition(&playerPos);
		cameraPosZ = player->pos.z;
		cameraPosX = player->pos.x;
		cameraPosY = player->pos.y;
	}

	xyz->x = targetPosX - cameraPosX;
	xyz->y = targetPosY - cameraPosY;
	xyz->z = targetPosZ - cameraPosZ;

	return true;
}

float NormalAbsoluteAngle(float angle)
{
	while (angle < 0)
		angle += 2 * PI;
	while (angle > 2 * PI)
		angle -= 2 * PI;
	return angle;
}

float NormalRelativeAngle(float angle)
{
	while (angle > PI)
		angle -= 2 * PI;
	while (angle < -PI)
		angle += 2 * PI;
	return angle;
}

void ComputeAnglesFromMatrix(NiMatrix33* mat, NiPoint3* angle)
{
	const float threshold = 0.001;
	if (abs(mat->data[2][1] - 1.0) < threshold)
	{
		angle->x = PI / 2;
		angle->y = 0;
		angle->z = atan2(mat->data[1][0], mat->data[0][0]);
	}
	else if (abs(mat->data[2][1] + 1.0) < threshold)
	{
		angle->x = -PI / 2;
		angle->y = 0;
		angle->z = atan2(mat->data[1][0], mat->data[0][0]);
	}
	else
	{
		angle->x = asin(mat->data[2][1]);
		angle->y = atan2(-mat->data[2][0], mat->data[2][2]);
		angle->z = atan2(-mat->data[0][1], mat->data[1][1]);
	}
}

void GetCameraAngle(NiPoint3* pos)
{
	PlayerCamera* camera = PlayerCamera::GetSingleton();
	PlayerCharacter* player = *g_thePlayer;
	float x, y, z;

	if (IsCameraFirstPerson())
	{
		FirstPersonState* fps = (FirstPersonState*)camera->cameraState;
		NiPoint3 angle;
		ComputeAnglesFromMatrix(&fps->cameraNode->m_worldTransform.rot, &angle);
		z = NormalAbsoluteAngle(-angle.z);
		x = player->rot.x - angle.x;
		y = angle.y;
	}
	else if (IsCameraThirdPerson())
	{
		ThirdPersonState* fps = (ThirdPersonState*)camera->cameraState;
		NiPoint3 angle;
		z = player->rot.z + fps->diffRotZ;
		x = player->rot.x + fps->diffRotX;
		y = 0;
	}
	else
	{
		x = player->rot.x;
		y = player->rot.y;
		z = player->rot.z;
	}

	pos->x = x;
	pos->y = y;
	pos->z = z;
}

static void RotateCamera(TESObjectREFR* target)
{
	NiPoint3 pos;
	if (!CheckDistance(target, &pos))
		return;

	PlayerCharacter* player = *g_thePlayer;
	float x, y, z, xy;
	float rotZ, rotX;

	x = pos.x;
	y = pos.y;
	z = pos.z;

	xy = sqrt(x*x + y*y);
	rotZ = atan2(x, y);
	rotX = atan2(-z, xy);

	float angleDiffZ, angleDiffX;
	{
		angleDiffZ = rotZ - player->rot.z;
		angleDiffX = rotX - player->rot.x;

		while (angleDiffZ < -PI)
			angleDiffZ += 2 * PI;
		while (angleDiffZ > PI)
			angleDiffZ -= 2 * PI;
		while (angleDiffX < -PI)
			angleDiffX += 2 * PI;
		while (angleDiffX > PI)
			angleDiffX -= 2 * PI;
	}

	angleDiffZ = angleDiffZ / (Settings::fCameraSpeed * 60 / 2000);
	angleDiffX = angleDiffX / (Settings::fCameraSpeed * 60 / 2000);

	float angleZ, angleX;
	{
		angleZ = player->rot.z + angleDiffZ;
		angleX = player->rot.x + angleDiffX;

		while (angleZ < 0)
			angleZ += 2 * PI;
		while (angleZ > 2 * PI)
			angleZ -= 2 * PI;
		while (angleX < -PI)
			angleX += 2 * PI;
		while (angleX > PI)
			angleX -= 2 * PI;
	}

	rotZ = angleZ;
	rotX = angleX;

	PlayerCamera * camera = PlayerCamera::GetSingleton();
	if ((player->actorState.flags04 & 0x0003C000) == 0)
	{
		if (IsCameraThirdPerson())
		{
			ThirdPersonState* tps = (ThirdPersonState*)camera->cameraState;
			tps->diffRotZ = 0.0;
			tps->diffRotX = 0.0;

			SetAngleZ(player, rotZ);
			SetAngleX(player, rotX);
		}
		else if (IsCameraFirstPerson())
		{
			TESCameraController* controller = TESCameraController::GetSingleton();

			controller->startRotZ = player->rot.z;
			controller->startRotX = player->rot.x;
			controller->endRotZ = rotZ;
			controller->endRotX = rotX;
			controller->unk14 = Settings::fCameraSpeed;
			controller->unk1C = 0;
		}
		else
		{
			SetAngleZ(player, rotZ);
			SetAngleX(player, rotX);
		}
	}
/*	else
	{
		if (IsCameraFirstPerson())
		{
			FirstPersonState* fps = (FirstPersonState*)camera->cameraState;

			float targetPosZ = atan2(x, y);
			_MESSAGE("atan2(x, y) %f", targetPosZ);

			while (targetPosZ < 0)
				targetPosZ += 2 * PI;
			while (targetPosZ > 2 * PI)
				targetPosZ -= 2 * PI;
			while (curRotZ < 0)
				curRotZ += 2 * PI;
			while (curRotZ > 2 * PI)
				curRotZ -= 2 * PI;

			_MESSAGE("curRotZ %f, targetPosZ %f, playerRotZ %f", curRotZ, targetPosZ, player->GetActorRotationZ(0));
			float rotZ = targetPosZ - player->GetActorRotationZ(0);
			float diffRotZ = abs(rotZ) / (Settings::fCameraSpeed * 60 / 1000);

			if (curRotZ >= targetPosZ)
			{
				if (fps->sittingRot < rotZ)
					fps->sittingRot -= 0;
				else
					fps->sittingRot -= diffRotZ;
			}
			else
			{
				if (fps->sittingRot > rotZ)
					fps->sittingRot += 0;
				else
					fps->sittingRot += diffRotZ;
			}

			SetAngleX(player, rotX);
		}
		else if (IsCameraThirdPerson())
		{
			ThirdPersonState* tps = (ThirdPersonState*)camera->cameraState;

			tps->diffRotZ = atan2(x, y) - camera->unk154;
			tps->diffRotX = 0;
		}
		else
		{
			SetAngleZ(player, rotZ);
			SetAngleX(player, rotX);
		}
	}
*/
}

TESObjectWEAP * OnCameraMove(PlayerCharacter * player, bool isLeftHand)
{
	if (g_fovStep > 0)
	{
		PlayerCamera* camera = PlayerCamera::GetSingleton();
		if (g_fovStep == 1)
		{
			afterThis = true;
			camera->worldFOV = g_worldfovTo;;
			camera->firstPersonFOV = g_firstfovTo;
		}
		else
		{
			float diff = g_worldfovTo - camera->worldFOV;
			camera->worldFOV += diff / g_fovStep;
			float diff2 = g_firstfovTo - camera->firstPersonFOV;
			camera->firstPersonFOV += diff2 / g_fovStep;
			if (refTarget)
			{
				RotateCamera(refTarget);
			}
		}
		g_fovStep--;
	}

	if (refTarget && afterThis && (g_fovStep <= 1))
	{
		if (Settings::bLockOn)
			RotateCamera(refTarget);
		else
		{
			NiPoint3 pos;
			CheckDistance(refTarget, &pos);

			NiPoint3 cameraAngle;
			GetCameraAngle(&cameraAngle);
			float angleZ = NormalRelativeAngle(atan2(pos.x, pos.y) - cameraAngle.z);
			float angleX = NormalRelativeAngle(atan2(-pos.z, sqrt(pos.x*pos.x + pos.y*pos.y)) - cameraAngle.x);

			float crosshairLoc = sqrt(angleZ*angleZ + angleX*angleX);

			if (crosshairLoc >= 0.01)
				RotateCamera(refTarget);
			else{
				refTarget = NULL;
				afterThis = false;
			}
		}
	}

	return GetEquippedWeaponFunc(player, isLeftHand);
}

void DisablePlayerLook(bool isGamepadDisabled)
{
	PlayerControls * pcControls = PlayerControls::GetSingleton();

	if (Settings::bLockOn)
		pcControls->unk1D8 = 0;
	else
		pcControls->unk1D8 = isGamepadDisabled;
}

bool Hooks_F2F_Commit()
{
	{
		struct InstallHookOnCameraMove_Code : Xbyak::CodeGenerator {
			InstallHookOnCameraMove_Code(void * buf, uintptr_t funcAddr) : Xbyak::CodeGenerator(4096, buf)
			{
				Xbyak::Label retnLabel;
				Xbyak::Label funcLabel;

				sub(rsp, 0x20);

				xor(edx, edx);
				mov(rcx, rdi);
				call(ptr[rip + funcLabel]);

				add(rsp, 0x20);

				jmp(ptr[rip + retnLabel]);

				L(funcLabel);
				dq(funcAddr);

				L(retnLabel);
				dq(kOnCameraMoveAddr.GetUIntPtr() + 0x5);
			}
		};

		void * codeBuf = g_localTrampoline.StartAlloc();
		InstallHookOnCameraMove_Code code(codeBuf, GetFnAddr(&OnCameraMove));
		g_localTrampoline.EndAlloc(code.getCurr());

		if (!g_branchTrampoline.Write5Branch(kOnCameraMoveAddr.GetUIntPtr(), uintptr_t(code.getCode())))
			return false;
	}

	{
		struct InstallHookDisablePlayerLook_Code : Xbyak::CodeGenerator {
			InstallHookDisablePlayerLook_Code(void * buf, uintptr_t funcAddr) : Xbyak::CodeGenerator(4096, buf)
			{
				Xbyak::Label retnLabel;
				Xbyak::Label funcLabel;

				sub(rsp, 0x20);

				mov(cl, al);
				call(ptr[rip + funcLabel]);

				add(rsp, 0x20);

				jmp(ptr[rip + retnLabel]);

				L(funcLabel);
				dq(funcAddr);

				L(retnLabel);
				dq(kDisablePlayerLookAddr.GetUIntPtr() + 0x6);
			}
		};

		void * codeBuf = g_localTrampoline.StartAlloc();
		InstallHookDisablePlayerLook_Code code(codeBuf, GetFnAddr(&DisablePlayerLook));
		g_localTrampoline.EndAlloc(code.getCurr());

		if (!g_branchTrampoline.Write6Branch(kDisablePlayerLookAddr.GetUIntPtr(), uintptr_t(code.getCode())))
			return false;
	}

	return true;
}

extern "C"
{

	bool SKSEPlugin_Query(const SKSEInterface * skse, PluginInfo * info)
	{

		gLog.OpenRelative(CSIDL_MYDOCUMENTS, "\\My Games\\Skyrim Special Edition\\SKSE\\SimpleFace2FaceConv.log");

		info->infoVersion = PluginInfo::kInfoVersion;
		info->name = "Simple Face2Face Conversation plugin";
		info->version = 3;

		g_pluginHandle = skse->GetPluginHandle();

		if (skse->isEditor)
		{
			_MESSAGE("loaded in editor, marking as incompatible");
			return false;
		}

		if (skse->runtimeVersion != RUNTIME_VERSION_1_5_39)
		{
			_MESSAGE("This plugin is not compatible with this versin of game.");
			return false;
		}

		if (!g_branchTrampoline.Create(1024 * 64))
		{
			_ERROR("couldn't create branch trampoline. this is fatal. skipping remainder of init process.");
			return false;
		}

		if (!g_localTrampoline.Create(1024 * 64, g_moduleHandle))
		{
			_ERROR("couldn't create codegen buffer. this is fatal. skipping remainder of init process.");
			return false;
		}

		return true;
	}

	bool SKSEPlugin_Load(const SKSEInterface * skse)
	{
		_MESSAGE("Load");

		if (Hooks_F2F_Commit()){
			_MESSAGE("Hook Succeeded");
		}
		else{
			_MESSAGE("Hook Failed");
			return false;
		}

		ConsoleCommand::Register();

		g_messaging = (SKSEMessagingInterface*)skse->QueryInterface(kInterface_Messaging);
		bool bMessaging = g_messaging->RegisterListener(g_pluginHandle, "SKSE", MessageHandler);
		if (bMessaging) {
			_MESSAGE("Event Sinked");
		}
		else{
			return false;
		}

		return true;
	}

}