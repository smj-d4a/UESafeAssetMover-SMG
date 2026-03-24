// Copyright FB Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Framework/Docking/TabManager.h"

/**
 * SafeAssetMover 독립 에디터 탭 창 등록 및 관리
 */
class SAFEASSETMOVER_API FSafeAssetMoverWindow
{
public:
	static const FName TabId;

	/** 탭 스포너 등록 */
	static void Register();

	/** 탭 스포너 해제 */
	static void Unregister();

	/** 창 열기 (선택적으로 소스 폴더 미리 설정) */
	static void Open(const FString& PresetSourceFolder = TEXT(""));

	/** 탭 스폰 콜백 (RegisterNomadTabSpawner에 전달) */
	static TSharedRef<class SDockTab> SpawnTab(const class FSpawnTabArgs& Args);
};
