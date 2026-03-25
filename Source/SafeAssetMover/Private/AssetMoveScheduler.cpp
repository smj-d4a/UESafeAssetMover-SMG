// Copyright FB Team. All Rights Reserved.

#include "AssetMoveScheduler.h"
#include "AssetDependencyAnalyzer.h"

void FAssetMoveScheduler::ComputeMoveOrder(
	TArray<TSharedPtr<FAssetMoverEntry>>& InOutEntries,
	const FDependencyGraph& Graph)
{
	if (InOutEntries.Num() == 0) return;

	TMap<FName, int32> PkgToIndex;
	for (int32 i = 0; i < InOutEntries.Num(); ++i)
	{
		PkgToIndex.Add(InOutEntries[i]->AssetData.PackageName, i);
	}

	TMap<FName, int32> InDegree;
	for (const TSharedPtr<FAssetMoverEntry>& Entry : InOutEntries)
	{
		InDegree.FindOrAdd(Entry->AssetData.PackageName) = 0;
	}

	for (const TSharedPtr<FAssetMoverEntry>& Entry : InOutEntries)
	{
		const FName PkgName = Entry->AssetData.PackageName;
		if (const TSet<FName>* InternalDeps = Graph.InternalDeps.Find(PkgName))
		{
			for (const FName& Dep : *InternalDeps)
			{
				if (InDegree.Contains(Dep))
				{
					InDegree[PkgName]++;
				}
			}
		}
	}

	TQueue<FName> Queue;
	for (const auto& Pair : InDegree)
	{
		if (Pair.Value == 0)
		{
			Queue.Enqueue(Pair.Key);
		}
	}

	int32 OrderCounter = 0;
	TArray<FName> Sorted;
	TSet<FName>  SortedSet;

	while (!Queue.IsEmpty())
	{
		FName Current;
		Queue.Dequeue(Current);
		Sorted.Add(Current);
		SortedSet.Add(Current);

		int32* EntryIdx = PkgToIndex.Find(Current);
		if (EntryIdx && InOutEntries.IsValidIndex(*EntryIdx))
		{
			InOutEntries[*EntryIdx]->MoveOrder = OrderCounter++;
		}

		if (const TSet<FName>* Refs = Graph.Referencers.Find(Current))
		{
			for (const FName& Ref : *Refs)
			{
				if (int32* Deg = InDegree.Find(Ref))
				{
					(*Deg)--;
					if (*Deg == 0)
					{
						Queue.Enqueue(Ref);
					}
				}
			}
		}
	}

	// 순환 의존성 처리 (O(1) lookup)
	if (Sorted.Num() < InOutEntries.Num())
	{
		UE_LOG(LogSafeAssetMover, Warning,
			TEXT("Cyclic dependencies detected. %d assets will be moved last."),
			InOutEntries.Num() - Sorted.Num());

		for (TSharedPtr<FAssetMoverEntry>& Entry : InOutEntries)
		{
			const FName PkgName = Entry->AssetData.PackageName;
			if (!SortedSet.Contains(PkgName))
			{
				Entry->MoveOrder = OrderCounter++;
				Entry->bHasCyclicDep = true;
			}
		}
	}
}

void FAssetMoveScheduler::SortByMoveOrder(TArray<TSharedPtr<FAssetMoverEntry>>& InOutEntries)
{
	InOutEntries.Sort([](const TSharedPtr<FAssetMoverEntry>& A, const TSharedPtr<FAssetMoverEntry>& B)
	{
		return A->MoveOrder < B->MoveOrder;
	});
}
