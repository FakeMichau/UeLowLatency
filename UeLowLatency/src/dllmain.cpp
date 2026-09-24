#include <DynamicOutput/Output.hpp>
#include <Mod/CppUserModBase.hpp>
#include <Unreal/UObjectGlobals.hpp>
#include <Unreal/UObject.hpp>
#include <Unreal/Hooks/Hooks.hpp>
#include <UEngine.hpp>
#include <Unreal/UKismetSystemLibrary.hpp>
#include <Unreal/UFunction.hpp>
#include <Unreal/UClass.hpp>

namespace Mod
{
	using namespace RC;
	using namespace Unreal;

	typedef void(*PFN_FLTickExternal)(int64_t frameId, float DeltaSeconds, bool bIdleMode);
	typedef void(*PFN_CameraUpdateExternal)(int64_t frameId, float cameraPosition[3], float cameraRotation[3], float fovAngle);
	typedef int64(*PFN_GetFrameCount)();

	/**
	* UeLowLatency: UE4SS c++ mod class defintion
	*/
	class UeLowLatency : public RC::CppUserModBase {
	public:

		// constructor
		UeLowLatency() {
			ModVersion = STR("0.2");
			ModName = STR("UeLowLatency");
			ModAuthors = STR("FakeMichau");
			ModDescription = STR("A proxy for getting into sim thread");
			// Do not change this unless you want to target a UE4SS version
			// other than the one you're currently building with somehow.
			//ModIntendedSDKVersion = STR("2.6");

			Output::send<LogLevel::Warning>(STR("[UeLowLatency]: Init.\n"));
		}

		// destructor
		~UeLowLatency() override {
			// fill when required
		}

		auto on_program_start() -> void override
		{
		}

		auto on_dll_load(std::wstring_view dll_name) -> void override
		{
		}

		static inline UKismetSystemLibrary* kismetSystemLibrary = nullptr;
		static inline UFunction* GetFrameCount = nullptr;

		static inline UObject* playerController = nullptr;
		static inline UObject* playerCameraManager = nullptr;
		static inline UFunction* GetCameraRotation = nullptr;
		static inline UFunction* GetCameraLocation = nullptr;
		static inline UFunction* GetFOVAngle = nullptr;

		auto on_unreal_init() -> void override
		{
			kismetSystemLibrary = UObjectGlobals::StaticFindObject<UKismetSystemLibrary*>(nullptr, nullptr, STR("/Script/Engine.Default__KismetSystemLibrary"));

			if (kismetSystemLibrary) {
				Output::send<LogLevel::Verbose>(STR("kismetSystemLibrary non null\n"));

				GetFrameCount = kismetSystemLibrary->GetFunctionByName(STR("GetFrameCount"));
			}

			// That's the generic class, not the active object, we only grab methods here
			// We grab the playerController on OnClientRestart and then the active playerCameraManager in PostEngineTick
			auto tempCameraManager = UObjectGlobals::StaticFindObject<UObject*>(nullptr, nullptr, STR("/Script/Engine.PlayerCameraManager"));
			if (tempCameraManager) {
				Output::send<LogLevel::Verbose>(STR("tempCameraManager non null\n"));
				GetCameraRotation = tempCameraManager->GetFunctionByName(STR("GetCameraRotation"));
				GetCameraLocation = tempCameraManager->GetFunctionByName(STR("GetCameraLocation"));
				GetFOVAngle = tempCameraManager->GetFunctionByName(STR("GetFOVAngle"));
			}

			UFunction* ClientRestart = UObjectGlobals::StaticFindObject<UFunction*>(nullptr, nullptr, STR("/Script/Engine.PlayerController:ClientRestart"));
			if (ClientRestart) {
				Output::send<LogLevel::Verbose>(STR("Found ClientRestart. Registering hook.\n"));
				UObjectGlobals::RegisterHook(ClientRestart, OnClientRestart, nullptr, nullptr);
			}
			else {
				Output::send<LogLevel::Warning>(STR("Could not find ClientRestart.\n"));
			}

			Unreal::Hook::RegisterEngineTickPreCallback(PreEngineTick, { false, false, STR("UeLowLatency"), STR("PreEngineTick") });
			Unreal::Hook::RegisterEngineTickPostCallback(PostEngineTick, { false, false, STR("UeLowLatency"), STR("PostEngineTick") });
		}

		inline static std::mutex playerControllerMutex{};
		static void OnClientRestart(Unreal::UnrealScriptFunctionCallableContext& Context, void* CustomData)
		{
			std::scoped_lock lock(Mod::UeLowLatency::playerControllerMutex);
			playerController = Context.Context;
		}

		inline static PFN_FLTickExternal flTickStartExternal = nullptr;
		inline static std::mutex tickStartMutex{};

		static void PreEngineTick(Unreal::Hook::TCallbackIterationData<void>& CallbackIterationData, UEngine* Context, float DeltaSeconds, bool bIdleMode) {


			//uint8* smoothFrameRate = Context->GetValuePtrByPropertyNameInChain<uint8>(STR("bSmoothFrameRate"));

			//if (smoothFrameRate) {
			//	*smoothFrameRate = 0;
			//}

			int64 currentFrameId{};
			if (kismetSystemLibrary && GetFrameCount) {
				kismetSystemLibrary->ProcessEvent(GetFrameCount, &currentFrameId);
			}

			std::scoped_lock lock(Mod::UeLowLatency::tickStartMutex);
			if (flTickStartExternal)
				flTickStartExternal(currentFrameId, DeltaSeconds, bIdleMode);

			return;
		};

		inline static PFN_FLTickExternal flTickEndExternal = nullptr;
		inline static std::mutex tickEndMutex{};

		inline static PFN_CameraUpdateExternal cameraUpdateExternal = nullptr;
		inline static std::mutex cameraUpdateMutex{};

		static void PostEngineTick(Unreal::Hook::TCallbackIterationData<void>& CallbackIterationData, UEngine* Context, float DeltaSeconds, bool bIdleMode) {
			{
				std::scoped_lock lock(Mod::UeLowLatency::playerControllerMutex);
				static UObject* lastPlayerController = nullptr;
				if (lastPlayerController != playerController) {
					lastPlayerController = playerController;

					UObject** CameraManagerPtr = playerController->GetValuePtrByPropertyNameInChain<UObject*>(STR("PlayerCameraManager"));

					if (CameraManagerPtr && *CameraManagerPtr) {
						playerCameraManager = *CameraManagerPtr;
					}
				}
			}

			int64 currentFrameId{};
			if (kismetSystemLibrary && GetFrameCount) {
				kismetSystemLibrary->ProcessEvent(GetFrameCount, &currentFrameId);
			}

			FRotator currentRotation{};
			FVector currentLocation{};
			float cameraPosition[3];
			float cameraRotation[3]; // Pitch, Yaw, Roll
			if (playerCameraManager && GetCameraRotation && GetCameraLocation) {
				playerCameraManager->ProcessEvent(GetCameraRotation, &currentRotation);
				playerCameraManager->ProcessEvent(GetCameraLocation, &currentLocation);

				// UE4 uses float, UE5 double, calling those methods should avoid this causing an issue
				cameraPosition[0] = currentLocation.GetX();
				cameraPosition[1] = currentLocation.GetY();
				cameraPosition[2] = currentLocation.GetZ();

				cameraRotation[0] = currentRotation.GetPitch();
				cameraRotation[1] = currentRotation.GetYaw();
				cameraRotation[2] = currentRotation.GetRoll();

				float fovAngle = 0.0f;
				if (GetFOVAngle) {
					playerCameraManager->ProcessEvent(GetFOVAngle, &fovAngle);
				}

				std::scoped_lock lock(Mod::UeLowLatency::cameraUpdateMutex);
				if (cameraUpdateExternal)
					cameraUpdateExternal(currentFrameId, cameraPosition, cameraRotation, fovAngle);
			}

			std::scoped_lock lock(Mod::UeLowLatency::tickEndMutex);
			if (flTickEndExternal)
				flTickEndExternal(currentFrameId, DeltaSeconds, bIdleMode);

			return;
		};
	};
}

/**
* export the start_mod() and uninstall_mod() functions to
* be used by the core ue4ss system to load in our dll mod
*/
#define MOD_EXPORT __declspec(dllexport) 
extern "C" {
	MOD_EXPORT RC::CppUserModBase* start_mod() { return new Mod::UeLowLatency(); }
	MOD_EXPORT void uninstall_mod(RC::CppUserModBase* mod) { delete mod; }

	MOD_EXPORT void setTickStartCallback(Mod::PFN_FLTickExternal callback) {
		std::scoped_lock lock(Mod::UeLowLatency::tickStartMutex);
		Mod::UeLowLatency::flTickStartExternal = callback;
	}

	MOD_EXPORT void setTickEndCallback(Mod::PFN_FLTickExternal callback) {
		std::scoped_lock lock(Mod::UeLowLatency::tickEndMutex);
		Mod::UeLowLatency::flTickEndExternal = callback;
	}

	MOD_EXPORT void setCameraUpdateCallback(Mod::PFN_CameraUpdateExternal callback) {
		std::scoped_lock lock(Mod::UeLowLatency::cameraUpdateMutex);
		Mod::UeLowLatency::cameraUpdateExternal = callback;
	}
}
