/* SPDX-License-Identifier: MPL-2.0 */

#pragma once

#include "CoreMinimal.h"
#include "AkAudioEvent.h"
#include "Module/GameInstanceModule.h"
#include "Th3WwiseBrowserRootInstance.generated.h"

UCLASS(Abstract)
class TH3WWISEBROWSER_API UTh3WwiseBrowserRootInstance : public UGameInstanceModule
{
	GENERATED_BODY()
public:
	UPROPERTY(BlueprintReadWrite)
	TArray<UAkAudioEvent*> AudioEvents;

	std::atomic_bool bAudioEventsReady;

	UFUNCTION(BlueprintCallable, BlueprintPure = false)
	void GetFilteredEntries(TArray<UAkAudioEvent*>& out_FilteredEntries, const FString& SearchQuery) const;

	void ProcessOneAkAudioEvent(const FSoftObjectPath& SoftPath);

	virtual void DispatchLifecycleEvent(ELifecyclePhase Phase) override;
};
