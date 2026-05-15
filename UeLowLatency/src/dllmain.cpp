#include <DynamicOutput/Output.hpp>
#include <Mod/CppUserModBase.hpp>
#include <Unreal/UObjectGlobals.hpp>
#include <Unreal/UObject.hpp>
#include <Unreal/Hooks/Hooks.hpp>
#include <UEngine.hpp>
#include <Unreal/UKismetSystemLibrary.hpp>

namespace Mod
{
	using namespace RC;
	using namespace Unreal;

	typedef void(*PFN_FLTickExternal)(int64_t frameId, float DeltaSeconds, bool bIdleMode);
	typedef int64(*PFN_GetFrameCount)();

	/**
	* UeLowLatency: UE4SS c++ mod class defintion
	*/
	class UeLowLatency : public RC::CppUserModBase {
	public:

		// constructor
		UeLowLatency() {
			ModVersion = STR("0.1");
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

		auto on_unreal_init() -> void override
		{
			kismetSystemLibrary = UObjectGlobals::StaticFindObject<UKismetSystemLibrary*>(nullptr, nullptr, STR("/Script/Engine.Default__KismetSystemLibrary"));

			if (kismetSystemLibrary) {
				Output::send<LogLevel::Verbose>(STR("kismetSystemLibrary non null\n"));

				GetFrameCount = kismetSystemLibrary->GetFunctionByName(STR("GetFrameCount"));
			}

			Unreal::Hook::RegisterEngineTickPreCallback(PreEngineTick, { false, false, STR("UeLowLatency"), STR("PreEngineTick") });
			Unreal::Hook::RegisterEngineTickPostCallback(PostEngineTick, { false, false, STR("UeLowLatency"), STR("PostEngineTick") });
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

		static void PostEngineTick(Unreal::Hook::TCallbackIterationData<void>& CallbackIterationData, UEngine* Context, float DeltaSeconds, bool bIdleMode) {
			int64 currentFrameId{};

			if (kismetSystemLibrary && GetFrameCount) {
				kismetSystemLibrary->ProcessEvent(GetFrameCount, &currentFrameId);
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
}





