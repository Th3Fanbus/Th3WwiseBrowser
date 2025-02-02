/* SPDX-License-Identifier: MPL-2.0 */

#include "Th3WwiseBrowserRootInstance.h"
#include "Algo/Accumulate.h"
#include "Algo/AllOf.h"
#include "Algo/AnyOf.h"
#include "Algo/ForEach.h"
#include "Algo/Reverse.h"
#include "Algo/Transform.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogTh3WwiseBrowserCpp, Log, All);

#define TH3_PROJECTION_THIS(FuncName) \
	[this](auto&&... Args) -> decltype(auto) \
	{ \
		return FuncName(Forward<decltype(Args)>(Args)...); \
	}

void UTh3WwiseBrowserRootInstance::GetFilteredEntries(TArray<UAkAudioEvent*>& out_FilteredEntries, const FString& SearchQuery) const
{
	TArray<FString> SearchWords;
	SearchQuery.ParseIntoArrayWS(SearchWords);

	if (not bAudioEventsReady) {
		UE_LOG(LogTh3WwiseBrowserCpp, Error, TEXT("ENTRIES STILL NOT READY, THERE ARE %d ENTRIES"), AudioEvents.Num());
	}

	const TFunction<bool(const UAkAudioEvent*)> predicate_none = [](const UAkAudioEvent* AkEvent) {
		return true;
	};
	const TFunction<bool(const UAkAudioEvent*)> predicate_find = [&SearchWords](const UAkAudioEvent* AkEvent) {
		const FString Path = AkEvent->GetPathName();
		return Algo::AllOf(SearchWords, [Path](const FString& Word) { return Path.Contains(Word); });
	};
	Algo::TransformIf(AudioEvents, out_FilteredEntries, SearchWords.IsEmpty() ? predicate_none : predicate_find, FIdentityFunctor());
}

void UTh3WwiseBrowserRootInstance::ProcessOneAkAudioEvent(const FSoftObjectPath& SoftPath)
{
	UAkAudioEvent* AkEvent = TSoftObjectPtr<UAkAudioEvent>(SoftPath).Get();
	if (not AkEvent) {
		//UE_LOG(LogTh3WwiseBrowserCpp, Error, TEXT("Got nullptr Material"));
		return;
	}
	if (AkEvent->HasAnyFlags(RF_ClassDefaultObject)) {
		return;
	}
	UE_LOG(LogTh3WwiseBrowserCpp, Display, TEXT("Processing AkAudioEvent %s"), *AkEvent->GetName());

	AudioEvents.Add(AkEvent);
}

static void LoadAsync(UClass* BaseClass, const TFunction<void(const TArray<FSoftObjectPath>&)> Callback)
{
	const FString ClassName = BaseClass->GetName();
	UE_LOG(LogTh3WwiseBrowserCpp, Display, TEXT("Looking for '%s'..."), *ClassName);
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	TArray<FAssetData> AssetData;
	AssetRegistryModule.Get().GetAssetsByClass(FTopLevelAssetPath(BaseClass), AssetData, true);
	TArray<FSoftObjectPath> SoftPaths;
	const auto asset_predicate = [](const FAssetData& Asset) { return not Asset.PackageName.ToString().StartsWith(TEXT("/ControlRig")); };
	const auto asset_transform = [](const FAssetData& Asset) { return Asset.GetSoftObjectPath(); };
	Algo::TransformIf(AssetData, SoftPaths, asset_predicate, asset_transform);
	UE_LOG(LogTh3WwiseBrowserCpp, Display, TEXT("Loading %d '%s'..."), SoftPaths.Num(), *ClassName);
	const double Begin = FPlatformTime::Seconds();
	UAssetManager::GetStreamableManager().RequestAsyncLoad(SoftPaths, [Begin, ClassName, SoftPaths, Callback]() {
		const double Middle = FPlatformTime::Seconds();
		UE_LOG(LogTh3WwiseBrowserCpp, Warning, TEXT("Took %f ms to load %d '%s'"), (Middle - Begin) * 1000, SoftPaths.Num(), *ClassName);
		Invoke(Callback, SoftPaths);
		const double End = FPlatformTime::Seconds();
		UE_LOG(LogTh3WwiseBrowserCpp, Warning, TEXT("Took %f ms to process %d '%s'"), (End - Middle) * 1000, SoftPaths.Num(), *ClassName);
	}, FStreamableManager::AsyncLoadHighPriority);
}

void UTh3WwiseBrowserRootInstance::DispatchLifecycleEvent(ELifecyclePhase Phase)
{
	Super::DispatchLifecycleEvent(Phase);

	UE_LOG(LogTh3WwiseBrowserCpp, Display, TEXT("Dispatching Phase %s on %s"), *LifecyclePhaseToString(Phase), *this->GetPathName());

	if (Phase == ELifecyclePhase::POST_INITIALIZATION) {
		LoadAsync(UAkAudioEvent::StaticClass(), [this](const TArray<FSoftObjectPath>& Paths) {
			Algo::ForEach(Paths, TH3_PROJECTION_THIS(ProcessOneAkAudioEvent));
			Algo::Sort(AudioEvents, [](const UAkAudioEvent* A, const UAkAudioEvent* B) {
				return A->GetFName().Compare(B->GetFName()) < 0;
			});
			bAudioEventsReady = true;
		});
	}
}