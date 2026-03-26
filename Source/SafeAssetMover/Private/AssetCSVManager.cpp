// Copyright FB Team. All Rights Reserved.

#include "AssetCSVManager.h"
#include "AssetDependencyAnalyzer.h"
#include "Misc/FileHelper.h"

#define LOCTEXT_NAMESPACE "FAssetCSVManager"

bool FAssetCSVManager::ExportCSV(
	const FString& CSVFilePath,
	const TArray<TSharedPtr<FAssetMoverEntry>>& Entries)
{
	TArray<FString> Lines;
	Lines.Add(TEXT("SourcePath,TargetPath,AssigneeIndex"));

	for (const TSharedPtr<FAssetMoverEntry>& Entry : Entries)
	{
		if (!Entry.IsValid()) continue;

		// 쉼표가 포함된 경로는 따옴표로 감쌈
		const FString SrcEscaped = Entry->SourcePath.Contains(TEXT(","))
			? FString::Printf(TEXT("\"%s\""), *Entry->SourcePath)
			: Entry->SourcePath;
		const FString TgtEscaped = Entry->TargetPath.Contains(TEXT(","))
			? FString::Printf(TEXT("\"%s\""), *Entry->TargetPath)
			: Entry->TargetPath;

		Lines.Add(FString::Printf(TEXT("%s,%s,%d"),
			*SrcEscaped,
			*TgtEscaped,
			Entry->AssigneeIndex));
	}

	const FString Content = FString::Join(Lines, TEXT("\n"));
	const bool bOK = FFileHelper::SaveStringToFile(
		Content, *CSVFilePath,
		FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);

	if (bOK)
	{
		UE_LOG(LogSafeAssetMover, Log,
			TEXT("[CSV] %d 항목 내보내기 완료: %s"), Entries.Num(), *CSVFilePath);
	}
	else
	{
		UE_LOG(LogSafeAssetMover, Warning,
			TEXT("[CSV] 파일 저장 실패: %s"), *CSVFilePath);
	}
	return bOK;
}

int32 FAssetCSVManager::ImportCSV(
	const FString& CSVFilePath,
	TArray<TSharedPtr<FAssetMoverEntry>>& Entries)
{
	FString Content;
	if (!FFileHelper::LoadFileToString(Content, *CSVFilePath))
	{
		UE_LOG(LogSafeAssetMover, Warning,
			TEXT("[CSV] 파일 읽기 실패: %s"), *CSVFilePath);
		return 0;
	}

	// SourcePath → Entry 빠른 조회 맵
	TMap<FString, TSharedPtr<FAssetMoverEntry>> PathMap;
	for (TSharedPtr<FAssetMoverEntry>& Entry : Entries)
	{
		if (Entry.IsValid())
		{
			PathMap.Add(Entry->SourcePath, Entry);
		}
	}

	TArray<FString> Lines;
	Content.ParseIntoArrayLines(Lines, /*bCullEmpty=*/true);

	int32 UpdatedCount = 0;

	for (int32 i = 1; i < Lines.Num(); ++i) // 헤더 행 건너뜀
	{
		// 간단한 CSV 파싱 (따옴표 처리 포함)
		TArray<FString> Cols;
		FString Line = Lines[i];

		// 따옴표로 감싼 필드를 단순하게 처리
		// 경로에 쉼표가 없으면 간단 분리
		Line.ParseIntoArray(Cols, TEXT(","), /*bCullEmpty=*/false);
		if (Cols.Num() < 2) continue;

		FString SrcPath = Cols[0].TrimStartAndEnd().TrimChar('"');
		FString TgtPath = Cols[1].TrimStartAndEnd().TrimChar('"');
		const int32 AssigneeIdx = (Cols.Num() >= 3)
			? FCString::Atoi(*Cols[2].TrimStartAndEnd())
			: -1;

		if (TSharedPtr<FAssetMoverEntry>* Found = PathMap.Find(SrcPath))
		{
			(*Found)->TargetPath    = TgtPath;
			(*Found)->AssigneeIndex = AssigneeIdx;
			++UpdatedCount;
		}
	}

	UE_LOG(LogSafeAssetMover, Log,
		TEXT("[CSV] %d / %d 항목 업데이트됨 (파일: %s)"),
		UpdatedCount, Lines.Num() - 1, *CSVFilePath);
	return UpdatedCount;
}

#undef LOCTEXT_NAMESPACE
